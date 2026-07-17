#include"../include/AIUtil/AIHelper.h"
#include"../include/AIUtil/MQManager.h"
#include <stdexcept>
#include<chrono>

// 构造函数
AIHelper::AIHelper() {
    //默认使用阿里云大模型  1 对应阿里云大模型
    strategy = StrategyFactory::instance().create("1"); // StrategyFactory::instance() 获取单例工厂对象，create("1") 创建默认的AI策略对象
}

// 设置指定的AI对象 
void AIHelper::setStrategy(std::shared_ptr<AIStrategy> strat) {
    strategy = strat;
}


// 设置默认模型
//void AIHelper::setModel(const std::string& modelName) {
  //  model_ = modelName;
//}

// 聊天记录核心
// 添加一条用户消息
void AIHelper::addMessage(int userId,const std::string& userName, bool is_user,const std::string& userInput, std::string sessionId) {
    auto now = std::chrono::system_clock::now();  // 获取当前时间点
    auto duration = now.time_since_epoch();       // 计算从1970年1月1日到现在的时间间隔 纳秒级别
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(); // 将高精度的纳秒截取成毫秒， 使用count()将这个时间对象转变为long long 放进数据库
    messages.push_back({ userInput,ms });  // 将用户内容和时间戳放进AIHelper的内存容器中，使得AI有短期记忆 
    //消息队列异步入库
    pushMessageToMysql(userId, userName, is_user, userInput, ms, sessionId);
}

void AIHelper::restoreMessage(const std::string& userInput,long long ms) {
    messages.push_back({ userInput,ms }); // 将聊天消息从数据库加载到message中 比如重启服务器 内存清空，这个函数帮助AI恢复记忆
    //优化空间： push_back()->emplace_back() 从先构造一个临时对象在移动（拷贝） 变成 原地构造
}


// 发送聊天消息
std::string AIHelper::chat(int userId,std::string userName, std::string sessionId, std::string userQuestion, std::string modelType) {
// 整个系统的核心大脑 接受用户提问，通过判断模型是否支持MCP（工具调用）协议，决定直接回答还是走一套ReAct双阶段交互流程调用外部工具（查天气等），最终给用户返回整合后的智能回答
    //设置策略
    setStrategy(StrategyFactory::instance().create(modelType)); // 根据用户传来的模型通过单例工厂动态创建对应的AI策略，绑定到当前实例上

    // 路径A：不支持MCP 普通对话
    if (false == strategy->isMCPModel) {// 细节： 将常量写在左边 --> 尤达条件式 防止程序员将 == 写成 =  这样写成 = 编译失败

        addMessage(userId, userName, true, userQuestion, sessionId); // 将用户提问写进内存 异步的写进数据库
        json payload = strategy->buildRequest(this->messages); // 使用内存中的 message构建标准的JSON请求体 payload

        //执行请求
        json response = executeCurl(payload); // 通过函数executeCurl()发送请求
        std::string answer = strategy->parseResponse(response); // 提取纯文本（string）的回答
        addMessage(userId, userName, false, answer, sessionId); // 将AI回答放进内存message 和 数据库（异步）
        return answer.empty() ? "[Error] 无法解析响应" : answer;
    }
    // 路径B：
    //说明支持MCP
    AIConfig config; // 实例化 配置器
    config.loadFromFile("../AIApps/ChatServer/resource/config.json"); // 从文件中加载配置文件 可能有API配置 工具描述 Prompt模板
    std::string tempUserQuestion =config.buildPrompt(userQuestion); // 将用户的提问 和 配置文件中的系统提示词 工具Schema描述列表拼在一起
    std::cout << "tempUserQuestion is " << tempUserQuestion << std::endl;
    messages.push_back({ tempUserQuestion, 0 }); // 0 占位时间戳 因为这是临时构建的过渡期Probe探针 询问模型是否需要调用工具，用完马上pop 

    json firstReq = strategy->buildRequest(this->messages); // 向模型发起提问
    json firstResp = executeCurl(firstReq); 
    std::string aiResult = strategy->parseResponse(firstResp);// 得到模型的回答（包含模型决定是否使用工具）
    // 用完立即移除提示词
    messages.pop_back();

    std::cout << "aiResult is " << aiResult << std::endl;
    // 解析AI响应（是否工具调用）
    AIToolCall call = config.parseAIResponse(aiResult); // 解析模型回答

    // 情况1：AI 不调用工具
    if (!call.isToolCall) {
        addMessage(userId, userName, true, userQuestion, sessionId);
        addMessage(userId, userName, false, aiResult, sessionId);

        std::cout << "No tools required" << std::endl;
        return aiResult; // 与普通回答一致
    }

    // 情况 2：AI 要调用工具
    json toolResult; // 接收工具返回值
    AIToolRegistry registry; // 实例化工具注册中心

    try {
        toolResult = registry.invoke(call.toolName, call.args); // 通过invole调度外部工具（工具名， 参数）
        std::cout << "Tool call success" << std::endl;
    }
    catch (const std::exception& e) {
        //大多数情况都不会走这里
        std::string err = "[工具调用失败] " + std::string(e.what());
        addMessage(userId, userName, true, userQuestion, sessionId);
        addMessage(userId, userName, false, err, sessionId);

        std::cout << "Tool call failed" << std::endl << std::string(e.what());
        return err;
    }



    // ReAct模式关键： 将工具执行结果反馈给模型
    // 第二次调用AI
    // 用同样的 prompt_template，但说明工具执行过
    std::string secondPrompt = config.buildToolResultPrompt(userQuestion, call.toolName, call.args, toolResult);
    
    std::cout << "secondPrompt is " << secondPrompt << std::endl;
    messages.push_back({ secondPrompt, 0 }); // 临时探针 使用 0 占位时间戳

    json secondReq = strategy->buildRequest(messages);
    json secondResp = executeCurl(secondReq);
    std::string finalAnswer = strategy->parseResponse(secondResp);
    //删除包含提示词的信息
    messages.pop_back();

    std::cout << "finalAnswer is " << finalAnswer << std::endl;

    addMessage(userId, userName, true, userQuestion, sessionId);
    addMessage(userId, userName, false, finalAnswer, sessionId);
    return finalAnswer;

}

// 发送自定义请求体
json AIHelper::request(const json& payload) {
    return executeCurl(payload);
}

std::vector<std::pair<std::string, long long>> AIHelper::GetMessages() {
    return this->messages;
}


// 内部方法：执行 curl 请求
json AIHelper::executeCurl(const json& payload) {
// 用C++发起HTTP POST请求，将JSON发给AI服务商的服务器（阿里云/Deepseek/等），拿到返回的JSON数据 是项目的通信引擎
    //curl/libcurl 发送网路请求的工具/库 ---见文档
    CURL* curl = curl_easy_init(); // 函数curl_easy_init()用于创建curl会话对象
    if (!curl) {
        throw std::runtime_error("Failed to initialize curl");
    }

    std::cout<<"test "<< strategy->getApiUrl().c_str()<<' '<< strategy->getApiKey()<<std::endl;

    std::string readBuffer;
    struct curl_slist* headers = nullptr;
    std::string authHeader = "Authorization: Bearer " + strategy->getApiKey(); // 构建 密钥 访问AI接口的“门票”

    // curl_slist_append() libcurl要求把多个header组织成一个"链表" 将密钥 和 body类型 追加到链表中
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string payloadStr = payload.dump(); // HTTP对body的要求是纯文本  dump()是将JSON序列转化为字符串


    // curl_easy_setopt()  是libcurl的核心函数，用来"给这次请求设置各种参数"
    curl_easy_setopt(curl, CURLOPT_URL, strategy->getApiUrl().c_str()); // 目标网址
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);                // 添加headers
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());     // 设置body
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);       // 明确收到返回数据是调用的回调函数 重要
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);             // 传给回调的"上下文数据",在readBuffer中 

    // 明确： HTTP响应数据不是一次性到达，而是分小块(chunk) 陆续到达

    // CURLcode res = curl_easy_perform(curl);
    // curl_slist_free_all(headers);
    // curl_easy_cleanup(curl);

    // 真正将url发出的代码 
    // curl_easy_perform() 是阻塞的（同步调用） 程序会卡在这里，直到拿到完整的AI服务器响应或出错才会向下执行
    CURLcode res = curl_easy_perform(curl); // res 放的是这次请求成功与否的状态码（是curl自己的状态码）

    std::cout << "\n========== Request ==========\n";
    std::cout << payload.dump(4) << std::endl;

    std::cout << "\n========== Raw Response ==========\n";
    std::cout << readBuffer << std::endl;
    std::cout << "===============================\n";

    // 清理资源 非常重要！
    curl_slist_free_all(headers); // 上面申请的header链表资源是手动分配的内存 必须使用curl_slist_free_all()释放 否则内存泄漏
    curl_easy_cleanup(curl);      //  curl对象同理 需要使用curl_easy_cleanup()释放

    if (res != CURLE_OK) { // 请求失败
        throw std::runtime_error("curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)));
    }

    try {
        return json::parse(readBuffer); // 返回服务器返回的内容（纯文本） 使用parse()将纯文本转换为json
    }
    catch (...) {
        throw std::runtime_error("Failed to parse JSON response: " + readBuffer);
    }
}

// curl 回调函数，把返回的数据写到 string buffer
// 参数时libcurl规定死的： 收到的这一块数据的起始地址 数据单元大小 数据单元个数 用户自定义的数据指针（传进来的&readBuffer）
size_t AIHelper::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* buffer = static_cast<std::string*>(userp); // libcurl的接口是通用的c接口 只能接受void*,所以回调函数需要将其转换为真是类型才能使用
    buffer->append(static_cast<char*>(contents), totalSize); // 将收到的数据追加到buffer中 ， 这里的buffer就是传进来的readBuffer
    return totalSize; // 返回本次数据的大小，“告诉curl我处理了多少字节”  libcurl规定：回调函数必须返回你实际处理（消费）的字节数
}
// 结合executeCurl和WriteCallback的结构图----见文档

std::string AIHelper::escapeString(const std::string& input) {
    // 对字符串做"转义"处理 将有特殊含义的字符替换成转义序列 防止特殊字符破坏后续的语法结构
    // 例如：用户： 我叫O'Brien  insert到message时会变成：'我叫O'Brien'   '' 结构被破坏
    std::string output;
    output.reserve(input.size() * 2);
    for (char c : input) {
        switch (c) {
            case '\\': output += "\\\\"; break;
            case '\'': output += "\\\'"; break;
            case '\"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:   output += c; break;
        }
    }
    return output;
}

// 将一条聊天消息拼装成SQL插入语句，丢进消息队列，交给异步任务写入MySQL数据库
void AIHelper::pushMessageToMysql(int userId, const std::string& userName, bool is_user, const std::string& userInput,long long ms, std::string sessionId) {
    // std::string sql = "INSERT INTO chat_message (id, username, is_user, content, ts) VALUES ("
    //     + std::to_string(userId) + ", "  // 这里用 userId 作为 id，或者你自己生成
    //     + "'" + userName + "', "
    //     + std::to_string(is_user ? 1 : 0) + ", "
    //     + "'" + userInput + "', "
    //     + std::to_string(ms) + ")";
    
    // 上面注释掉的就是没有经过转移函数的有漏洞的写法 
    
    // 对输入进行特殊字符转义 方式破坏后续语法
    std::string safeUserName = escapeString(userName); 
    std::string safeUserInput = escapeString(userInput);

    //拼装 sql语句
    std::string sql = "INSERT INTO chat_message (id, username, session_id, is_user, content, ts) VALUES ("
        + std::to_string(userId) + ", "
        + "'" + safeUserName + "', "
        + sessionId + ", "
        + std::to_string(is_user ? 1 : 0) + ", "
        + "'" + safeUserInput + "', "
        + std::to_string(ms) + ")";

    //改成消息队列异步执行mysql操作，用于流量削峰与解耦逻辑  详解 见文档
    //mysqlUtil_.executeUpdate(sql); // 直接将这条语句发送到数据库执行

    //将消息放进消息队列 sql_queue中排队 函数立即退出 不等待数据库执行完毕 异步
    MQManager::instance().publish("sql_queue", sql);
}


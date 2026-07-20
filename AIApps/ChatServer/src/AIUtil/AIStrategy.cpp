#include"../include/AIUtil/AIStrategy.h"
#include"../include/AIUtil/AIFactory.h"

// 以阿里云为例 后面的AI逻辑基本一致
std::string AliyunStrategy::getApiUrl() const { // 请求的网址
    return "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
}

std::string AliyunStrategy::getApiKey()const { // 不能写死 在构造函数中已经从环境变量中都出来了，这里直接返回即可
    return apiKey_;
}


std::string AliyunStrategy::getModel() const { // 设置模型名，联系buildRequest中的payload["model"] = getModel()
    return "qwen-plus";
}

// 构建请求
json AliyunStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    payload["model"] = getModel();
    json msgArray = json::array(); // 创建空JSON数组，专门存放所有对话消息

    for (size_t i = 0; i < messages.size(); ++i) { // 遍历每条消息
        json msg;
        if (i % 2 == 0) { // 偶数是用户说的话
            msg["role"] = "user";
        }
        else { // 奇数时AI说的话
            msg["role"] = "assistant";
        }
        msg["content"] = messages[i].first; // 将文本放进mas中的“content”字段
        msgArray.push_back(msg);
    }
    payload["messages"] = msgArray; // 将msgArray数组放进 payload的"messages"字段
    return payload;
}


// std::string AliyunStrategy::parseResponse(const json& response) const {
//     if (response.contains("choices") && !response["choices"].empty()) {
//         return response["choices"][0]["message"]["content"];
//     }
//     return {};
// }
std::string AliyunStrategy::parseResponse(const json& response) const
// 从AI的回答，JSON响应，中将AI回复的文字提取出来   输入：JSON响应  输出string 纯文本的响应内容
{
    std::cout << "\n========== AI Response ==========" << std::endl;
    std::cout << response.dump(4) << std::endl;
    std::cout << "=================================\n" << std::endl;

    // 正常返回
    // 一系列的安全检查
    if (response.contains("choices") &&                         // 检查有没有“choices”字段
        response["choices"].is_array() &&                       // 检查“choices”是不是数组 
        !response["choices"].empty() &&                         // 检查“choices”是否为空
        response["choices"][0].contains("message") &&           // 检查“choices”的首元素是不是“messages”
        response["choices"][0]["message"].contains("content"))  // 检查“choices”中的“messages”中有没有“content”
    {
        return response["choices"][0]["message"]["content"].get<std::string>(); // 有的话 返回“content”的string格式
    }

    // OpenAI/阿里兼容接口错误
    if (response.contains("error"))
    {
        std::cout << "AI Error:" << std::endl;
        std::cout << response["error"].dump(4) << std::endl;
    }

    std::cout << "parseResponse() failed: cannot find choices[0].message.content" << std::endl;

    return "";
}


std::string DouBaoStrategy::getApiUrl()const {
    return "https://ark.cn-beijing.volces.com/api/v3/chat/completions";
}

std::string DouBaoStrategy::getApiKey()const {
    return apiKey_;
}


std::string DouBaoStrategy::getModel() const {
    // return "doubao-seed-1-6-thinking-250715";
    return "doubao-seed-evolving";
}


json DouBaoStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    payload["model"] = getModel();
    json msgArray = json::array();

    for (size_t i = 0; i < messages.size(); ++i) {
        json msg;
        if (i % 2 == 0) {
            msg["role"] = "user";
        }
        else {
            msg["role"] = "assistant";
        }
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["messages"] = msgArray;
    return payload;
}


std::string DouBaoStrategy::parseResponse(const json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        return response["choices"][0]["message"]["content"];
    }
    return {};
}


std::string AliyunRAGStrategy::getApiUrl() const {
    const char* key = std::getenv("Knowledge_Base_ID");
    if (!key) throw std::runtime_error("Knowledge_Base_ID not found!");
    std::string id(key);
    //对应知识库id
    return "https://dashscope.aliyuncs.com/api/v1/apps/"+id+"/completion";
}

std::string AliyunRAGStrategy::getApiKey()const {
    return apiKey_;
}


std::string AliyunRAGStrategy::getModel() const {
    return ""; // 不需要模型
}


json AliyunRAGStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    json msgArray = json::array();
    for (size_t i = 0; i < messages.size(); ++i) {
        json msg;
        msg["role"] = (i % 2 == 0 ? "user" : "assistant");
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["input"]["messages"] = msgArray;
    payload["parameters"] = json::object(); 
    return payload;
}


std::string AliyunRAGStrategy::parseResponse(const json& response) const {
    if (response.contains("output") && response["output"].contains("text")) {
        return response["output"]["text"];
    }
    return {};
}



std::string AliyunMcpStrategy::getApiUrl() const {
    return "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
}

std::string AliyunMcpStrategy::getApiKey()const {
    return apiKey_;
}


std::string AliyunMcpStrategy::getModel() const {
    return "qwen-plus";
}


json AliyunMcpStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    payload["model"] = getModel();
    json msgArray = json::array();

    for (size_t i = 0; i < messages.size(); ++i) {
        json msg;
        if (i % 2 == 0) {
            msg["role"] = "user";
        }
        else {
            msg["role"] = "assistant";
        }
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["messages"] = msgArray;
    return payload;
}


std::string AliyunMcpStrategy::parseResponse(const json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        return response["choices"][0]["message"]["content"];
    }
    return {};
}

// 常见的“自注册”设计模式，将不同模型注册带工厂里的对应编号
// 这里的对象regAliyun、regDoubao、regAliyunRag、regAliyunMcp是静态对象，在程序启动时就会被创建，
// 调用构造函数注册策略到工厂中,这些变量本省后续不会再使用，只是为了使用构造函数创建对应策略
// 后续会move到class StrategyFactory的成员变量creators中，方便后续通过工厂创建对应策略实例
static StrategyRegister<AliyunStrategy> regAliyun("1");          // 阿里云       --> 1
static StrategyRegister<DouBaoStrategy> regDoubao("2");          // 豆包         --> 2
static StrategyRegister<AliyunRAGStrategy> regAliyunRag("3");    // 阿里云Rag    --> 3 
static StrategyRegister<AliyunMcpStrategy> regAliyunMcp("4");    // 阿里云Mcp    --> 4

//Rag Mcp 解释 见文档
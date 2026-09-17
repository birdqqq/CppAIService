#include"../include/AIUtil/AIStrategy.h"
#include"../include/AIUtil/AIFactory.h"
#include <cstdlib>

// 以阿里云为例 后面的AI逻辑基本一致
// 请求的网址
std::string AliyunStrategy::getApiUrl() const {
    return "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
}

// 不能写死 在构造函数中已经从环境变量中都出来了，这里直接返回即可
std::string AliyunStrategy::getApiKey()const {
    return apiKey_;
}


// 设置模型名，联系buildRequest中的payload["model"] = getModel()
std::string AliyunStrategy::getModel() const {
    return "qwen-plus";
}


// 构建请求
json AliyunStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    payload["model"] = getModel();
    // 创建空JSON数组，专门存放所有对话消息
    json msgArray = json::array();

    // 遍历每条消息
    for (size_t i = 0; i < messages.size(); ++i) {
        json msg;
        // 偶数是用户说的话
        if (i % 2 == 0) {
            msg["role"] = "user";
        }
        // 奇数时AI说的话
        else {
            msg["role"] = "assistant";
        }
        // 将文本放进mas中的“content”字段
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    // 将msgArray数组放进 payload的"messages"字段
    payload["messages"] = msgArray;
    return payload;
}


// 从AI的回答，JSON响应，中将AI回复的文字提取出来   输入：JSON响应  输出string 纯文本的响应内容
std::string AliyunStrategy::parseResponse(const json& response) const {
    // 正常返回
    // 一系列的安全检查
    // 检查有没有“choices”字段
    // 检查“choices”是否为空
    if (response.contains("choices") && !response["choices"].empty()) {
        // 有的话 返回“content”的string格式
        return response["choices"][0]["message"]["content"];
    }
    return {};
}


std::string DouBaoStrategy::getApiUrl()const {
    return "https://ark.cn-beijing.volces.com/api/v3/chat/completions";
}

std::string DouBaoStrategy::getApiKey()const {
    return apiKey_;
}


std::string DouBaoStrategy::getModel() const {
    // 模型名改为环境变量可配置(DOUBAO_MODEL)，默认用当前可用版本。
    // 原硬编码 doubao-seed-1-6-thinking-250715 已下线(Shutdown)。
    // 在火山方舟控制台开通对应模型后即可使用；换模型只需设环境变量，无需改代码重编译。
    // 同样 trim，避免环境变量里的尾随换行被拼进请求体（详见 EnvUtil.h）
    std::string model = getEnvTrimmed("DOUBAO_MODEL");
    if (!model.empty()) return model;
    return "doubao-seed-2-1-pro-260628";
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
    // 同样 trim：这个 id 会被直接拼进 URL，混入空白会导致请求地址错误（详见 EnvUtil.h）
    std::string id = requireEnvTrimmed("Knowledge_Base_ID", "Knowledge_Base_ID not found!");
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
// 阿里云       --> 1
static StrategyRegister<AliyunStrategy> regAliyun("1");
// 豆包         --> 2
static StrategyRegister<DouBaoStrategy> regDoubao("2");
// 阿里云Rag    --> 3
static StrategyRegister<AliyunRAGStrategy> regAliyunRag("3");
// 阿里云Mcp    --> 4
static StrategyRegister<AliyunMcpStrategy> regAliyunMcp("4");

//Rag Mcp 解释 见文档

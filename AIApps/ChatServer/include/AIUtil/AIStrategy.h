#pragma once
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <sstream>
#include <memory>

#include "../../../../HttpServer/include/utils/JsonUtil.h"



class AIStrategy {
// AI策略抽象基类 类似一本说明书 明确AI必须能做到下面这几件事：接口 自身不做实现
public:
    virtual ~AIStrategy() = default;

    // 纯虚函数 需要子类实现
    // APIַ
    virtual std::string getApiUrl() const = 0; // 获取请求的网址

    // API Key
    virtual std::string getApiKey() const = 0; // 获取访问AI接口的密钥


    virtual std::string getModel() const = 0; // 获取模型名称


    virtual json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const = 0; // 构建请求体 传入消息列表，返回json请求体


    virtual std::string parseResponse(const json& response) const = 0; // 解析响应体 传入json响应体，返回纯文本回答

    bool isMCPModel = false; // 标记是否支持MCP协议，默认false

};

// 阿里云策略
class AliyunStrategy : public AIStrategy {

public:
    AliyunStrategy() {
        const char* key = std::getenv("DASHSCOPE_API_KEY"); // API-KEY通过读取环境变量获得 而不是直接写死，因为API Key属于敏感信息，不能上传github
                                                            // 通过设置环境变量（bash）程序读取环境变量 这是企业项目的标准做法
        if (!key) throw std::runtime_error("Aliyun API Key not found!");
        apiKey_ = key;
        isMCPModel = false;
    }

    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    std::string getModel() const override;

    json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const override;
    std::string parseResponse(const json& response) const override;

private:
    std::string apiKey_;
};

// 豆包策略
class DouBaoStrategy : public AIStrategy {

public:
    DouBaoStrategy() {
        const char* key = std::getenv("DOUBAO_API_KEY");
        if (!key) throw std::runtime_error("DOUBAO API Key not found!");
        apiKey_ = key;
        isMCPModel = false;
    }
    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    std::string getModel() const override;

    json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const override;
    std::string parseResponse(const json& response) const override;

private:
    std::string apiKey_;
};

// RAG策略
class AliyunRAGStrategy : public AIStrategy {

public:
    AliyunRAGStrategy() {
        const char* key = std::getenv("DASHSCOPE_API_KEY");
        if (!key) throw std::runtime_error("Aliyun API Key not found!");
        apiKey_ = key;
        isMCPModel = false;
    }

    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    std::string getModel() const override;

    json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const override;
    std::string parseResponse(const json& response) const override;

private:
    std::string apiKey_;
};

// MCP策略
class AliyunMcpStrategy : public AIStrategy {

public:
    AliyunMcpStrategy() {
        const char* key = std::getenv("DASHSCOPE_API_KEY");
        if (!key) throw std::runtime_error("Aliyun API Key not found!");
        apiKey_ = key;
        isMCPModel = true;
    }

    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    std::string getModel() const override;

    json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const override;
    std::string parseResponse(const json& response) const override;

private:
    std::string apiKey_;
};








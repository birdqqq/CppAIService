#pragma once
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <sstream>
#include <memory>

#include "../../../../HttpServer/include/utils/JsonUtil.h"
#include "../../../../HttpServer/include/utils/EnvUtil.h"



class AIStrategy {
// AI策略抽象基类 类似一本说明书 明确AI必须能做到下面这几件事：接口 自身不做实现
public:
    virtual ~AIStrategy() = default;

    
    // 纯虚函数 需要子类实现
    // API地址
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
        // API-KEY通过读取环境变量获得 而不是直接写死，因为API Key属于敏感信息，不能上传github
        // 通过设置环境变量（bash）程序读取环境变量 这是企业项目的标准做法
        // 用 requireEnvTrimmed 而不是裸 getenv：它会自动去掉 docker -e / 读文件等方式带进来的
        // 尾随换行。换行会混进 Authorization 头导致请求头提前截断，服务端误判为没有请求体（详见 EnvUtil.h）
        apiKey_ = requireEnvTrimmed("DASHSCOPE_API_KEY", "Aliyun API Key not found!");
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
        // 同样 trim：豆包 Key 也常从文件/环境变量传入，避免尾随换行污染 Authorization 头
        apiKey_ = requireEnvTrimmed("DOUBAO_API_KEY", "DOUBAO API Key not found!");
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
        // 同 AliyunStrategy：trim 掉环境变量可能带进来的尾随换行（详见 EnvUtil.h）
        apiKey_ = requireEnvTrimmed("DASHSCOPE_API_KEY", "Aliyun API Key not found!");
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
        // 同 AliyunStrategy：trim 掉环境变量可能带进来的尾随换行（详见 EnvUtil.h）
        apiKey_ = requireEnvTrimmed("DASHSCOPE_API_KEY", "Aliyun API Key not found!");
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








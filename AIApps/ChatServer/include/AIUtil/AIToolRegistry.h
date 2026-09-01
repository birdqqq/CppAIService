#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <iostream>
#include <ctime>
#include <curl/curl.h>
#include "../../../../HttpServer/include/utils/JsonUtil.h"

class AIToolRegistry {
public:
    // 类型别名：例如private中的getWeather getTime都是ToolFunc类型的函数对象,
    using ToolFunc = std::function<json(const json&)>; // 使用 std::function 而不是 函数指针，因为函数指针不能很好的支持Lambda

    AIToolRegistry();

    void registerTool(const std::string& name, ToolFunc func); // 注册函数 将工具函数注册到tools_中
    json invoke(const std::string& name, const json& args) const;
    bool hasTool(const std::string& name) const;

private:
    // 工具表，初始化之后不会再修改，所有工具都是在构造函数中注册的 
    std::unordered_map<std::string, ToolFunc> tools_; // 工具注册表，存储工具名称和对应的函数对象 使用map查找方便,O(1)时间复杂度

    // 工具函数
    // 将工具函数设置为static，因为它们不依赖于类的实例状态，可以直接通过类名调用，而不需要创建对象实例 -- 见文档
    // curl 回调函数，把返回的数据写到 string buffer
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output);
    // 获取天气函数 
    static json getWeather(const json& args);
    // 获取时间函数
    static json getTime(const json& args);
};

#include "../include/AIUtil/AIToolRegistry.h"
#include <sstream>

// 构造函数 AIToolRegistry() 用于初始化工具注册表，注册了两个工具：get_weather 和 get_time
AIToolRegistry::AIToolRegistry() {
    registerTool("get_weather", getWeather);
    registerTool("get_time", getTime);
}

// 注册工具函数，用于在工具注册表中注册一个工具
void AIToolRegistry::registerTool(const std::string& name, ToolFunc func) {
    tools_[name] = func;
}

// 接受一个工具名称和参数，查找并调用注册的工具
json AIToolRegistry::invoke(const std::string& name, const json& args) const {
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        throw std::runtime_error("Tool not found: " + name);
    }
    return it->second(args); // 调用注册的工具函数，并传入参数
}

// 用于检查工具注册表中是否存在指定名称的工具
bool AIToolRegistry::hasTool(const std::string& name) const {
    return tools_.count(name) > 0;
}

// 用于处理从 CURL 接收到的数据
size_t AIToolRegistry::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

// 获取天气
json AIToolRegistry::getWeather(const json& args) {
    if (!args.contains("city")) {
        return json{ {"error", "Missing parameter: city"} };
    }

    std::string city = args["city"].get<std::string>(); // 取出city对应的值(此时还是json类型)，转成string
    std::string encodedCity;

    // URL 编码  为什么需要：URL（网址）里只能包含特定的安全字符（英文字母、数字、少数符号）。如果你直接把中文"北京"塞进网址里,读取失败
    // curl_easy_escape() 函数用于对字符串进行 URL 编码，返回一个新的字符串，即“北京” --> %E5%8C%97%E4%BA%AC 这种形式
    // curl_easy_escape() 见文档
    char* encoded = curl_easy_escape(nullptr, city.c_str(), city.length()); 
    if (encoded) {
        encodedCity = encoded;
        curl_free(encoded);     // 注意：curl_easy_escape() 返回的字符串需要使用 curl_free() 释放内存，否则会造成内存泄漏
    }
    else {
        return json{ {"error", "URL encode failed"} };
    }

    // 拼接完整的请求网址
    // 注： wttr.in 是一个免费的天气查询服务，支持多种语言和格式。这里使用了 format=3 参数，表示返回简洁的天气信息。 
    std::string url = "https://wttr.in/" + encodedCity + "?format=3&lang=zh";

    // 初始化 CURL CURL 是 libcurl 库中代表"一次网络请求会话"的类型，你可以把它想象成"打开一个浏览器窗口，准备去访问网页"。
    CURL* curl = curl_easy_init(); // curl_easy_init() 就是"创建这个窗口"，返回一个指针 curl，指向这次请求需要用到的所有内部数据。
    std::string response; // 用于存储从 wttr.in 返回的天气信息

    if (!curl) {
        return json{ {"error", "Failed to init CURL"} };
    }

    // 配置请求中的各种选项
    // curl_easy_setopt 这个函数专门用来"设置这次请求的各种参数选项"
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str()); // 设置请求的网址
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback); // 设置回调函数，告诉 CURL 收到数据后应该调用哪个函数来处理这些数据
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response); // 设置回调函数的上下文数据，这里传入 response 的地址，回调函数会把收到的数据写入这个字符串中 
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L); // 设置请求超时时间为 5 秒，防止请求长时间挂起
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // 设置允许重定向，wttr.in 可能会返回 302 重定向，告诉 CURL 自动跟随重定向

    // 发送请求
    CURLcode res = curl_easy_perform(curl); // 执行请求，阻塞直到请求完成或出错，返回一个 CURLcode 类型的状态码，表示请求的结果
    curl_easy_cleanup(curl); // 清理 CURL 对象，释放资源，防止内存泄漏 请求做完了（不管成功还是失败），curl 这个对象占用的资源就该释放掉了 

    if (res != CURLE_OK) {
        return json{ {"error", "CURL request failed"} };
    }

    // 返回天气信息
    return json{ {"city", city}, {"weather", response} };
}

// 获取当前时间
json AIToolRegistry::getTime(const json& args) {
    // 获取时间不需要参数 但是为了保持接口一致性，仍然接受一个 json 参数
    // 此时编译器会弹出警告
    // 使用(void)args; 来告诉编译器我们知道这个参数没有被使用，避免警告
    (void)args;
    std::time_t t = std::time(nullptr); // 获取当前时间的时间戳（自1970年1月1日以来的秒数）std::time_t C++ 标准库里表示"时间"的一种类型
    std::tm* now = std::localtime(&t); // std::tm 是 C++ 标准库里表示"时间"的另一种类型，包含年、月、日、时、分、秒等信息。std::localtime() 将时间戳转换为本地时间的 std::tm 结构体指针
    char buffer[64]; // 用于存储格式化后的时间字符串，64 字节足够存放 "YYYY-MM-DD HH:MM:SS" 这种格式的时间字符串
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", now); // std::strftime() 用于将 std::tm 结构体格式化为字符串，格式化规则由第三个参数指定，这里使用 "%Y-%m-%d %H:%M:%S" 表示 "年-月-日 时:分:秒" 的格式
    return json{ {"time", buffer} };
}

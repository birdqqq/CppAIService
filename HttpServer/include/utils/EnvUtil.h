#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <string>

// 读取环境变量，并去掉首尾空白。
//
// 为什么要 trim 而不是直接 std::getenv：
// 环境变量的值经常在不知不觉中带上尾随换行。例如
//     docker run -e KEY="$(cat key.txt)"      （文件本身以换行结尾时）
//     docker run -e KEY=$'...\n'
//     从配置/密钥文件里 read 出来再 export
// 都会把结尾的 \n 一起传进来。
//
// 这个 \n 的危害：它会被原样拼进 HTTP 头部的值里（"Authorization: Bearer " + key），
// 而 curl_slist 中的头部值一旦含 \n，请求头就会在该处提前终止 —— 后面的
// Content-Type / Content-Length 会落到请求体的位置，服务端于是认为「没有请求体」，
// 返回 400 "Request body is required."。
// 只看请求内容完全正常，必须看原始字节才能发现，排查成本极高，所以在入口统一收口。
inline std::string getEnvTrimmed(const char *name)
{
    const char *v = std::getenv(name);
    if (v == nullptr)
    {
        return std::string();
    }

    std::string s(v);
    // 判断「不是空白字符」，用作查找第一个/最后一个有效字符的谓词
    auto notSpace = [](unsigned char c) { return std::isspace(c) == 0; };
    // 去掉头部空白
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    // 去掉尾部空白：反向查找得到的 base() 是「最后一个有效字符的下一个位置」
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

// 同上，但取不到或 trim 后为空时抛出异常（用于必填的密钥类配置）
inline std::string requireEnvTrimmed(const char *name, const char *errMsg)
{
    std::string s = getEnvTrimmed(name);
    if (s.empty())
    {
        throw std::runtime_error(errMsg);
    }
    return s;
}

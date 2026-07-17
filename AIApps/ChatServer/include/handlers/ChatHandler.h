#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../ChatServer.h"

class ChatHandler : public http::router::RouterHandler
// 负责处理用户进入 AI 聊天页面的请求，验证用户登录状态，并返回 AI.html 页面
// 策略模式 开闭原则  见文档
{
public:
    explicit ChatHandler(ChatServer* server) : server_(server) {} // explicit 关键字表示构造函数只能显式调用，防止隐式转换

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;//override 关键字表示重写基类的虚函数
private:

private:
    ChatServer* server_;
    http::MysqlUtil     mysqlUtil_;
};

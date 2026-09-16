#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"

#include"../AIUtil/AISessionIdGenerator.h"
#include "../ChatServer.h"

//发送消息时会返回sessionId以及响应

class ChatCreateAndSendHandler : public http::router::RouterHandler
{
public:
    explicit ChatCreateAndSendHandler(ChatServer* server) : server_(server) {}

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:

private:
    ChatServer* server_;
    http::MysqlUtil     mysqlUtil_;
};

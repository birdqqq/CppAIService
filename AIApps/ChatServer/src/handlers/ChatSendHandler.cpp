#include "../include/handlers/ChatSendHandler.h"


void ChatSendHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {

        auto session = server_->getSessionManager()->getSession(req, resp);
        LOG_INFO << "session->getValue(\"isLoggedIn\") = " << session->getValue("isLoggedIn");
        if (session->getValue("isLoggedIn") != "true")
        {

            json errorResp;
            errorResp["status"] = "error";
            errorResp["message"] = "Unauthorized";
            std::string errorBody = errorResp.dump(4);

            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized,
                "Unauthorized", true, "application/json", errorBody.size(),
                errorBody, resp);
            return;
        }


        int userId = std::stoi(session->getValue("userId"));
        std::string username = session->getValue("username");

        std::string userQuestion; // 存用户的提问
        std::string modelType;    // 存用户选择的模型类型
        std::string sessionId;    // 存用户选择的会话id

        auto body = req.getBody();
        if (!body.empty()) {
            auto j = json::parse(body); // 解析请求体为JSON对象
            if (j.contains("question")) userQuestion = j["question"];  // 获取用户提问
            if (j.contains("sessionId")) sessionId = j["sessionId"];   // 获取用户选择的会话id

            modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "1";  // 获取用户选择的模型类型，默认值为"1"
        }


        std::shared_ptr<AIHelper> AIHelperPtr;  // 存放用户对应的AIHelper对象
        {
            std::lock_guard<std::mutex> lock(server_->mutexForChatInformation);  // 加锁，确保线程安全

            // chatInformation是两层字典结构 用户id->{session_id->AIHelper} 
            auto& userSessions = server_->chatInformation[userId];  // 根据获取用户的所有会话信息 

            if (userSessions.find(sessionId) == userSessions.end()) { // 没找到就是用这个新的helper

                userSessions.emplace( 
                    sessionId,
                    std::make_shared<AIHelper>()
                );
            }
            AIHelperPtr= userSessions[sessionId];  // 获取对应的AIHelper对象
        }
        

        std::string aiInformation=AIHelperPtr->chat(userId, username,sessionId, userQuestion, modelType);  // 调用AIHelper的chat方法，获取AI的响应内容
        // 将AI的响应内容封装成JSON格式，返回给前端
        json successResp;
        successResp["success"] = true;
        successResp["Information"] = aiInformation;
        std::string successBody = successResp.dump(4);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
        return;
    }
    catch (const std::exception& e)
    {

        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
    }
}










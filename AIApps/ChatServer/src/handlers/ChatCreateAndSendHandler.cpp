#include "../include/handlers/ChatCreateAndSendHandler.h"
#include <algorithm>


void ChatCreateAndSendHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
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

        std::string userQuestion;
        std::string modelType;

        auto body = req.getBody();
        if (!body.empty()) {
            auto j = json::parse(body);
            if (j.contains("question")) userQuestion = j["question"];


            modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "1";
        }

        AISessionIdGenerator generator;
        std::string sessionId = generator.generate();
        std::cout<<"ɵsessionIdΪ "<<sessionId<<std::endl;


        // 获取或创建会话上下文（不存在则懒加载最近消息，内存只保留滑动窗口）
        std::shared_ptr<AIHelper> AIHelperPtr = server_->getOrCreateAIHelper(userId, sessionId);

        // 新会话 id 记入会话列表（幂等，避免重复）
        {
            std::lock_guard<std::mutex> lock(server_->mutexForSessionsId);
            auto& ids = server_->sessionsIdsMap[userId];
            if (std::find(ids.begin(), ids.end(), sessionId) == ids.end())
            {
                ids.push_back(sessionId);
            }
        }

        std::string aiInformation=AIHelperPtr->chat(userId, username,sessionId, userQuestion, modelType);
        json successResp;
        successResp["success"] = true;
        successResp["Information"] = aiInformation;
        successResp["sessionId"] = sessionId;
        
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










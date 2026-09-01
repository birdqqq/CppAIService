#include "../include/handlers/ChatHistoryHandler.h"

void ChatHistoryHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
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

        std::string sessionId;
        int page = 1;
        int pageSize = 20;

        auto body = req.getBody();
        if (!body.empty()) {
            auto j = json::parse(body);
            if (j.contains("sessionId")) sessionId = j["sessionId"];
            if (j.contains("page")) page = j["page"].get<int>();
            if (j.contains("pageSize")) pageSize = j["pageSize"].get<int>();
        }

        if (page < 1) page = 1;
        if (page > 100000) page = 100000;
        if (pageSize <= 0) pageSize = 20;
        if (pageSize > 100) pageSize = 100;
        int offset = (page - 1) * pageSize;

        // 完整历史以 MySQL 为真相源，按 session 分页返回（不再受内存滑动窗口限制）
        long long total = 0;
        sql::ResultSet* cntRes = server_->mysqlUtil_.executeQuery(
            "SELECT COUNT(*) AS cnt FROM chat_message WHERE id = ? AND session_id = ?",
            userId, sessionId);
        if (cntRes->next()) total = cntRes->getInt64("cnt");

        json successResp;
        successResp["success"] = true;
        successResp["total"] = total;
        successResp["page"] = page;
        successResp["pageSize"] = pageSize;
        successResp["history"] = json::array();

        // LIMIT/OFFSET 内联为字面量：本项目 bindParams 全部按字符串绑定，MySQL 不接受字符串 LIMIT
        std::string sql = "SELECT is_user, content FROM chat_message "
            "WHERE id = ? AND session_id = ? ORDER BY ts ASC, id ASC LIMIT "
            + std::to_string(pageSize) + " OFFSET " + std::to_string(offset);
        sql::ResultSet* res = server_->mysqlUtil_.executeQuery(sql, userId, sessionId);
        while (res->next()) {
            json msgJson;
            msgJson["is_user"] = (res->getInt("is_user") == 1);
            msgJson["content"] = res->getString("content");
            successResp["history"].push_back(msgJson);
        }

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










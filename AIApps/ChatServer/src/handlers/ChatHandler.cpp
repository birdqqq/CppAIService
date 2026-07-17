
#include "../include/handlers/ChatHandler.h"

void ChatHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{

    try
    {

        auto session = server_->getSessionManager()->getSession(req, resp);
        LOG_INFO << "session->getValue(\"isLoggedIn\") = " << session->getValue("isLoggedIn");
        if (session->getValue("isLoggedIn") != "true") // 登陆失败
        {
            // 打印错误信息

            json errorResp;
            errorResp["status"] = "error";
            errorResp["message"] = "Unauthorized";
            std::string errorBody = errorResp.dump(4);

            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized,
                "Unauthorized", true, "application/json", errorBody.size(),
                errorBody, resp);
            return;
        }


        // 登陆成功
        int userId = std::stoi(session->getValue("userId"));
        std::string username = session->getValue("username");

        std::string reqFile("../AIApps/ChatServer/resource/AI.html"); // 获取登录页面
        FileUtil fileOperater(reqFile);
        if (!fileOperater.isValid())
        {
            LOG_WARN << reqFile << "not exist.";
            fileOperater.resetDefaultFile();
        }

        std::vector<char> buffer(fileOperater.size()); // 按照文件大小创建buffer
        fileOperater.readFile(buffer);  // 将文件写进buffer中 准备返给浏览器
        std::string htmlContent(buffer.data(), buffer.size());


        // 动态修改 HTML 
        size_t headEnd = htmlContent.find("</head>"); // 找到 head的位置
        if (headEnd != std::string::npos) // 找到了  std::string::npos 表示未找到
        {
            std::string script = "<script>const userId = '" + std::to_string(userId) + "';</script>";
            htmlContent.insert(headEnd, script); // 将用户id插入到head中 浏览器打开JS就能获取到用户id
        }

        // server_->packageResp(req.getVersion(), HttpResponse::k200Ok, "OK"
        //             , false, "text/html", htmlContent.size(), htmlContent, resp);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("text/html");
        resp->setContentLength(htmlContent.size());
        resp->setBody(htmlContent);
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



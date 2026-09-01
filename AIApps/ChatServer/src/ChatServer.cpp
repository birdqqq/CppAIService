#include "../include/handlers/ChatLoginHandler.h"
#include "../include/handlers/ChatRegisterHandler.h"
#include "../include/handlers/ChatLogoutHandler.h"
#include"../include/handlers/ChatHandler.h"
#include"../include/handlers/ChatEntryHandler.h"
#include"../include/handlers/ChatSendHandler.h"
#include"../include/handlers/AIMenuHandler.h"
#include"../include/handlers/AIUploadSendHandler.h"
#include"../include/handlers/AIUploadHandler.h"
#include"../include/handlers/ChatHistoryHandler.h"


#include"../include/handlers/ChatCreateAndSendHandler.h"
#include"../include/handlers/ChatSessionsHandler.h"
#include"../include/handlers/ChatSpeechHandler.h"

#include "../include/ChatServer.h"
#include "../../../HttpServer/include/http/HttpRequest.h"
#include "../../../HttpServer/include/http/HttpResponse.h"
#include "../../../HttpServer/include/http/HttpServer.h"
#include <thread>
#include <chrono>



using namespace http;


ChatServer::ChatServer(int port,
    const std::string& name,
    muduo::net::TcpServer::Option option)
    : httpServer_(port, name, option)
{
    initialize();
}

void ChatServer::initialize() {
    std::cout << "ChatServer initialize start  ! " << std::endl;
	http::MysqlUtil::init("tcp://127.0.0.1:3306", "root", "123456", "ChatHttpServer", 5);

    initializeSession();

    initializeMiddleware();

    initializeRouter();
}

void ChatServer::initChatMessage() {

    std::cout << "initChatMessage start ! " << std::endl;
    readDataFromMySQL();
    std::cout << "initChatMessage success ! " << std::endl;
}

void ChatServer::readDataFromMySQL() {

    // 只加载每个用户的会话 id 列表到内存；消息本体按需懒加载（见 loadSessionMessagesFromMysql）
    std::string sql = "SELECT DISTINCT id, session_id FROM chat_message ORDER BY id ASC, session_id ASC";

    sql::ResultSet* res;
    try {
        res = mysqlUtil_.executeQuery(sql);
    }
    catch (const std::exception& e) {
        std::cerr << "MySQL query failed: " << e.what() << std::endl;
        return;
    }

    while (res->next()) {
        long long user_id = 0;
        std::string session_id;

        try {
            user_id    = res->getInt64("id");
            session_id = res->getString("session_id");
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to read row: " << e.what() << std::endl;
            continue;
        }

        sessionsIdsMap[user_id].push_back(session_id);
    }

    std::cout << "readDataFromMySQL finished" << std::endl;
}



void ChatServer::setThreadNum(int numThreads) {
    httpServer_.setThreadNum(numThreads);
}


void ChatServer::start() {
    // 后台线程定期淘汰闲置的 AIHelper，避免上下文对象随会话数无限常驻内存
    std::thread([this] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::minutes(10));
            cleanIdleChatSessions(3600); // 1 小时未活跃即淘汰，跟随登录态
        }
    }).detach();

    httpServer_.start();
}

// 获取或创建用户的会话上下文；内存里没有则从 MySQL 懒加载最近消息
std::shared_ptr<AIHelper> ChatServer::getOrCreateAIHelper(int userId, const std::string& sessionId)
{
    std::lock_guard<std::mutex> lock(mutexForChatInformation);

    auto& userSessions = chatInformation[userId];
    auto it = userSessions.find(sessionId);
    if (it != userSessions.end())
    {
        return it->second;
    }

    auto helper = std::make_shared<AIHelper>();
    loadSessionMessagesFromMysql(userId, sessionId, helper, AIHelper::MAX_CONTEXT_MESSAGES);
    userSessions.emplace(sessionId, helper);
    return helper;
}

// 按会话从 MySQL 加载最近 limit 条消息（从旧到新），只放内存当热缓存
void ChatServer::loadSessionMessagesFromMysql(int userId, const std::string& sessionId,
    std::shared_ptr<AIHelper> helper, int limit)
{
    if (!helper || limit <= 0) return;

    try
    {
        // LIMIT 内联为字面量：bindParams 全部按字符串绑定，MySQL 不接受字符串 LIMIT
        std::string sql = "SELECT content, ts FROM ("
                          " SELECT content, ts FROM chat_message"
                          " WHERE id = ? AND session_id = ? ORDER BY ts DESC LIMIT "
                          + std::to_string(limit)
                          + ") t ORDER BY ts ASC";
        sql::ResultSet* res = mysqlUtil_.executeQuery(sql, userId, sessionId);
        while (res->next())
        {
            std::string content = res->getString("content");
            long long ts = res->getInt64("ts");
            helper->restoreMessage(content, ts);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "loadSessionMessagesFromMysql failed: " << e.what() << std::endl;
    }
}

// 淘汰超过 idleSeconds 未活跃的 AIHelper（内存中的 shared_ptr 释放，MySQL 历史不受影响）
void ChatServer::cleanIdleChatSessions(long long idleSeconds)
{
    std::lock_guard<std::mutex> lock(mutexForChatInformation);

    for (auto itUser = chatInformation.begin(); itUser != chatInformation.end(); )
    {
        auto& userSessions = itUser->second;
        for (auto itS = userSessions.begin(); itS != userSessions.end(); )
        {
            if (itS->second->idleForSeconds(idleSeconds))
            {
                itS = userSessions.erase(itS);
            }
            else
            {
                ++itS;
            }
        }

        if (userSessions.empty())
        {
            itUser = chatInformation.erase(itUser);
        }
        else
        {
            ++itUser;
        }
    }
}


void ChatServer::initializeRouter() {

    httpServer_.Get("/", std::make_shared<ChatEntryHandler>(this));
    httpServer_.Get("/entry", std::make_shared<ChatEntryHandler>(this));
    
    httpServer_.Post("/login", std::make_shared<ChatLoginHandler>(this));
    
    httpServer_.Post("/register", std::make_shared<ChatRegisterHandler>(this));
    
    httpServer_.Post("/user/logout", std::make_shared<ChatLogoutHandler>(this));

    httpServer_.Get("/chat", std::make_shared<ChatHandler>(this));

    httpServer_.Post("/chat/send", std::make_shared<ChatSendHandler>(this));
 
    httpServer_.Get("/menu", std::make_shared<AIMenuHandler>(this));
    
    httpServer_.Get("/upload", std::make_shared<AIUploadHandler>(this));
   
    httpServer_.Post("/upload/send", std::make_shared<AIUploadSendHandler>(this));
    
    httpServer_.Post("/chat/history", std::make_shared<ChatHistoryHandler>(this));

    
    httpServer_.Post("/chat/send-new-session", std::make_shared<ChatCreateAndSendHandler>(this));
    httpServer_.Get("/chat/sessions", std::make_shared<ChatSessionsHandler>(this));

    httpServer_.Post("/chat/tts", std::make_shared<ChatSpeechHandler>(this));
}

void ChatServer::initializeSession() {

    auto sessionStorage = std::make_unique<http::session::MemorySessionStorage>();

    auto sessionManager = std::make_unique<http::session::SessionManager>(std::move(sessionStorage));

    setSessionManager(std::move(sessionManager));
}

void ChatServer::initializeMiddleware() {

    auto corsMiddleware = std::make_shared<http::middleware::CorsMiddleware>();

    httpServer_.addMiddleware(corsMiddleware);
}


void ChatServer::packageResp(const std::string& version,
    http::HttpResponse::HttpStatusCode statusCode,
    const std::string& statusMsg,
    bool close,
    const std::string& contentType,
    int contentLen,
    const std::string& body,
    http::HttpResponse* resp)
{
    if (resp == nullptr)
    {
        LOG_ERROR << "Response pointer is null";
        return;
    }

    try
    {
        resp->setVersion(version);
        resp->setStatusCode(statusCode);
        resp->setStatusMessage(statusMsg);
        resp->setCloseConnection(close);
        resp->setContentType(contentType);
        resp->setContentLength(contentLen);
        resp->setBody(body);

        LOG_INFO << "Response packaged successfully";
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "Error in packageResp: " << e.what();

        resp->setStatusCode(http::HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Server Error");
        resp->setCloseConnection(true);
    }
}

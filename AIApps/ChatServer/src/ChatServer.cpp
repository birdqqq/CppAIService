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
    : httpServer_(port, name, option) // httpServer_ 是实例化的对象 需直接构造
{
    initialize();
}
//构造函数只负责 创建对象 而函数initialize负责初始化 职责分离

void ChatServer::initialize() {
    std::cout << "ChatServer initialize start  ! " << std::endl;
	http::MysqlUtil::init("tcp://127.0.0.1:3306", "root", "123456", "ChatHttpServer", 5); //先初始化数据库，因为登录注册聊天均需要数据库，所以第一个初始化

    initializeSession(); // 第二个初始化会话 因为后面的handler可能会访问session

    initializeMiddleware(); // 第三个初始化 因为，所有请求进入handler之前都会，经过中间件

    initializeRouter(); // 最后初始化router router需要创建ChatEntryHandler(this) 需要访问session 数据库等 所以最后初始化
}

void ChatServer::initChatMessage() {

    std::cout << "initChatMessage start ! " << std::endl;
    readDataFromMySQL();  // 服务器启动后先恢复上下文  假设服务器重启前有上下文信息，重启后没有恢复则全部丢失！不符合使用常识
    std::cout << "initChatMessage success ! " << std::endl;
}

void ChatServer::readDataFromMySQL() {
// 一句话总结：
// 从MySQL的chat_message表中把每个用户的聊天记录读出来，只把「用户id -> 会话id列表」重建到内存sessionsIdsMap中，
// 让服务器随时知道每个用户有哪些会话；消息内容不在这里加载，等用户真正打开某个会话时再按需懒加载

    // 这句查询语句 见笔记
    // 只加载每个用户的会话 id 列表到内存；消息本体按需懒加载（见 loadSessionMessagesFromMysql）
    std::string sql = "SELECT DISTINCT id, session_id FROM chat_message ORDER BY id ASC, session_id ASC";

    sql::ResultSet* res; // 实例化MySQL连接库，用于接收查询结果 一张查询结果表 类似Excel，一行一条记录，可一行行查询
    try {
        res = mysqlUtil_.executeQuery(sql); // 将查询语句交给数据库执行，结果用res接收
    }
    catch (const std::exception& e) {
        std::cerr << "MySQL query failed: " << e.what() << std::endl;
        return;
    }

    // 一行行遍历查询结果
    while (res->next()) {
        long long user_id = 0;
        std::string session_id;

        try {
            // 使用对应方法获取对应字段
            user_id    = res->getInt64("id");
            session_id = res->getString("session_id");
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to read row: " << e.what() << std::endl;
            continue;
        }

        sessionsIdsMap[user_id].push_back(session_id); // 将此次会话加到这个用户id下
    }

    std::cout << "readDataFromMySQL finished" << std::endl;
}



// ------------------
// 包装（wrapper）函数，将参数numThreads转发至httpserver的setThreadNum()函数
void ChatServer::setThreadNum(int numThreads) {
    httpServer_.setThreadNum(numThreads);
}


// 包装（wrapper）函数,同理，只负责转发（另外还会拉起后台的闲置会话清理线程）
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
// 这样做的目的：main()不直接依赖HttpServer 需要ChatServer进行转发 main()不用关心内部是不是httpserver，这就是封装带来的好处 ！
// -----------------

// 获取或创建用户的会话上下文；内存里没有则从 MySQL 懒加载最近消息
std::shared_ptr<AIHelper> ChatServer::getOrCreateAIHelper(int userId, const std::string& sessionId)
{
    std::lock_guard<std::mutex> lock(mutexForChatInformation);

    // chatInformation是两层字典结构 用户id->{session_id->AIHelper}
    auto& userSessions = chatInformation[userId]; //  userSessions保存的是该用户的所有会话
    auto it = userSessions.find(sessionId); //找出指定session_id的内容
    if (it != userSessions.end())
    {
        return it->second; // 找到了 helper就直接接收原有的AIHelper
    }

    // 没找到就是用这个新的helper
    auto helper = std::make_shared<AIHelper>(); // 指向一个新的AIHelper对象
    loadSessionMessagesFromMysql(userId, sessionId, helper, AIHelper::MAX_CONTEXT_MESSAGES);
    userSessions.emplace(sessionId, helper); // 加到字典里
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
            helper->restoreMessage(content, ts); // 使用restoreMessage传入这条消息的内容和时间
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
// 一句话总结：
// 是整个项目的路由注册中心 将URL和对应的处理器绑定
// 如果后续想添加新的接口，1.新建ChatXXHandler 2.在initializeRouter中注册 ！
// 举例:
//  httpServer_.Post("/chat/XX",std::make_shared<ChatXXHandler>(this));

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
    //初始化会话
    //分层设计  因为会话属于Http层，所以这里将SessionManager交给HttpServer管理

    auto sessionStorage = std::make_unique<http::session::MemorySessionStorage>();

    auto sessionManager = std::make_unique<http::session::SessionManager>(std::move(sessionStorage));//移动语义，转移所有权

    setSessionManager(std::move(sessionManager));
}

void ChatServer::initializeMiddleware() {
    //初始化中间件
    //本函数 给服务器增加CORS中间件
    //后续想增加日志中间件等其他中间件 统一在这里注册！

    auto corsMiddleware = std::make_shared<http::middleware::CorsMiddleware>();

    httpServer_.addMiddleware(corsMiddleware);
}


void ChatServer::packageResp(const std::string& version, // 版本号 (1.1?)
    http::HttpResponse::HttpStatusCode statusCode,       // 状态码 (200 ?)
    const std::string& statusMsg,                        // 状态信息 (OK ?)
    bool close,                                          // 是否关闭连接
    const std::string& contentType,                      // 内容类型 (application/json ?)
    int contentLen,                                      // 内容长度
    const std::string& body,                             // 响应体
    http::HttpResponse* resp)                            // 封装的位置 HttpResponse* resp 指针 指向HttpResponse
// 一句话总结：
// 负责统一构造HTTP响应
// 因为每个Handler处理完之后都需要将响应封装成HTTP，将这部分逻辑分离出来，当后面HTTP响应格式变化时，只改一处即可！
// 一个重要的变成原则 DRY （Don't Repeat Yourself，别重复自己）
{
    if (resp == nullptr)
    {
        LOG_ERROR << "Response pointer is null";
        return;
    }

    // 将对应内容 填到对应位置
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

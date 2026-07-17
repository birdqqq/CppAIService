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
// 将MySQL中的chat_message表中的所有历史聊天记录按时间顺序读出来，重建到内存chatInformation中，使服务器可以快速访问到聊天历史

    // 这句查询语句 见笔记
    std::string sql = "SELECT id, username,session_id, is_user, content, ts FROM chat_message ORDER BY ts ASC, id ASC";

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
        std::string session_id ;  
        std::string username, content;
        long long ts = 0;
        int is_user = 1; //标识 信息是AI发的 还是 用户发的

        try {
            // 使用对应方法获取对应字段
            user_id    = res->getInt64("id");       
            session_id = res->getString("session_id");  
            username   = res->getString("username");
            content    = res->getString("content");
            ts         = res->getInt64("ts");
            is_user    = res->getInt("is_user");
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to read row: " << e.what() << std::endl;
            continue; 
        }

        // chatInformation是两层字典结构 用户id->{session_id->AIHelper} 
        auto& userSessions = chatInformation[user_id];//  userSessions保存的是该用户的所有会话

        std::shared_ptr<AIHelper> helper; // 面对 没找到指定session_id的情况
        auto itSession = userSessions.find(session_id); //找出指定session_id的内容
        if (itSession == userSessions.end()) {// 没找到就是用这个新的helper
            helper = std::make_shared<AIHelper>(); // 指向一个新的AIHelper对象
            userSessions[session_id] = helper; // 加到字典里
        } else {
            helper = itSession->second; // 找到了 helper就直接接收原有的AIHelper
        }
        //因为 当用户打开之前的一个聊天时，正确的逻辑时将新的聊天加在原来的基础上
        

        sessionsIdsMap[user_id].push_back(session_id); // 将此次会话加到这个用户id下


        helper->restoreMessage(content, ts); // 使用restoreMessage传入这条消息的内容和时间
    }

    std::cout << "readDataFromMySQL finished" << std::endl;
}

// ------------------
// 包装（wrapper）函数，将参数numThreads转发至httpserver的setThreadNum()函数
void ChatServer::setThreadNum(int numThreads) {
    httpServer_.setThreadNum(numThreads);
}

// 包装（wrapper）函数,同理，只负责转发
void ChatServer::start() {
    httpServer_.start();
}
// 这样做的目的：main()不直接依赖HttpServer 需要ChatServer进行转发 main()不用关心内部是不是httpserver，这就是封装带来的好处 ！
// -----------------

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

#pragma once

#include <atomic>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <mutex>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>


#include "../../../HttpServer/include/http/HttpServer.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../../../HttpServer/include/utils/FileUtil.h"
#include "../../../HttpServer/include/utils/JsonUtil.h"
#include"AIUtil/AISpeechProcessor.h"
#include"AIUtil/AIHelper.h"
#include"AIUtil/ImageRecognizer.h"
#include"AIUtil/base64.h"
#include"AIUtil/MQManager.h"


class ChatLoginHandler;
class ChatRegisterHandler;
class ChatLogoutHandler;
class ChatHandler;
class ChatEntryHandler;
class ChatSendHandler;
class ChatHistoryHandler;

class AIMenuHandler;
class AIUploadHandler;
class AIUploadSendHandler;


class ChatCreateAndSendHandler;
class ChatSessionsHandler;
class ChatSpeechHandler;

class ChatServer {
public: //public就是对外暴露的接口
	ChatServer(int port,
		const std::string& name,
		muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort);

	void setThreadNum(int numThreads);
	void start();
	void initChatMessage();
	// 这四个接口 对应main()中的四个初始化内容
private:
	friend class ChatLoginHandler;
	friend class ChatRegisterHandler;
	friend  ChatLogoutHandler;
	friend class ChatHandler;
	friend class ChatEntryHandler;
	friend class ChatSendHandler;
	friend class AIMenuHandler;
	friend class AIUploadHandler;
	friend class AIUploadSendHandler;
	friend class ChatHistoryHandler;

	friend class ChatCreateAndSendHandler;
	friend class ChatSessionsHandler;
	friend class ChatSpeechHandler;

private:
// 初始化部分：
	void initialize();
	void initializeSession();
	void initializeRouter();
	void initializeMiddleware();
	
	// 初始化不同内容，使用不同的函数 单一职责原则 SRP

	void readDataFromMySQL();

	// 获取或创建会话的 AIHelper（不存在则懒加载最近消息）
	std::shared_ptr<AIHelper> getOrCreateAIHelper(int userId, const std::string& sessionId);
	// 按 session 从 MySQL 懒加载最近 limit 条消息进内存
	void loadSessionMessagesFromMysql(int userId, const std::string& sessionId,
		std::shared_ptr<AIHelper> helper, int limit);
	// 淘汰超过 idleSeconds 未活跃的 AIHelper（默认 1 小时，跟随登录态）
	void cleanIdleChatSessions(long long idleSeconds);

	void packageResp(const std::string& version, http::HttpResponse::HttpStatusCode statusCode,
		const std::string& statusMsg, bool close, const std::string& contentType,
		int contentLen, const std::string& body, http::HttpResponse* resp);

	void setSessionManager(std::unique_ptr<http::session::SessionManager> manager)
	{
		httpServer_.setSessionManager(std::move(manager));
	}
	http::session::SessionManager* getSessionManager() const
	{
		return httpServer_.getSessionManager();
	}

	http::HttpServer	httpServer_; // 整个服务器

	http::MysqlUtil		mysqlUtil_; // 数据库 登录 注册 聊天记录

	std::unordered_map<int, bool>	onlineUsers_; // 在线人数
	std::mutex	mutexForOnlineUsers_;

	

	// std::unordered_map<int, std::shared_ptr<AIHelper>> chatInformation;

	std::unordered_map<int, std::unordered_map<std::string,std::shared_ptr<AIHelper> > > chatInformation;
	// 负责           用户id             -->    多个AI聊天即session_id  --> 对应多个AIHelper
	std::mutex	mutexForChatInformation;

	std::unordered_map<int, std::shared_ptr<ImageRecognizer> > ImageRecognizerMap; // 图片识别 OCR 图片理解
	std::mutex	mutexForImageRecognizerMap;

	std::unordered_map<int,std::vector<std::string> > sessionsIdsMap; // 聊天历史 会话 RAG
	std::mutex mutexForSessionsId;

	//每个资源都配了一个锁，不同锁管理不同资源
};


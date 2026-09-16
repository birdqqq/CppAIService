#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <muduo/net/TcpServer.h>
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include"../include/ChatServer.h"

const std::string RABBITMQ_HOST = "localhost";
const std::string QUEUE_NAME = "sql_queue";
const int THREAD_NUM = 2;

void executeMysql(const std::string sql) {
    http::MysqlUtil mysqlUtil_;
    mysqlUtil_.executeUpdate(sql);
}


int main(int argc, char* argv[]) {
	//打印进程id方便调试 例如kill ... 杀死进程
	LOG_INFO << "pid = " << getpid();
	// 服务器名字
	std::string serverName = "ChatServer";
	// 默认监听80端口 通常会被命令行参数覆盖
	int port = 80;
    // 
    int opt;
    // 支持解析-p参数 eg: -p 8888
    const char* str = "p:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'p':
        {
            port = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
    // 设置日志级别 只打印warn及以上的日志 屏蔽第哦啊INFO DEBUG等 级别的日志
    muduo::Logger::setLogLevel(muduo::Logger::WARN);
    // 创建聊天服务器对象 参数：端口 服务器名称
    ChatServer server(port, serverName);
    // 设置IO线程数 = 4
    server.setThreadNum(4);
    
    // 睡眠等待两秒，等待ChatServer中构造函数的一些异步操作：
    // 连接数据库、链接Redis、初始化线程池
    // 等真正完成之后在继续执行
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 初始化聊天信息缓存 从MySQL中的chat_message表中，将聊天记录信息加载到内存中作为缓存，方便查找
    server.initChatMessage();    


    // 创建并启动RabbitMQ消费线程池
    // RabbitMQThreadPool是自定义吧类
    RabbitMQThreadPool pool(RABBITMQ_HOST, QUEUE_NAME, THREAD_NUM, executeMysql);
    // 封装了从"RabbitMQ消费消息+线程池处理消息"的逻辑
    // 启动线程池
    pool.start();

    // 启动服务器
    server.start();
}

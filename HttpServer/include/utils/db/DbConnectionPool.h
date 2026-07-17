#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <thread>
#include "DbConnection.h"

namespace http 
{
namespace db 
{

class DbConnectionPool 
{
public:
    // 单例模式
    static DbConnectionPool& getInstance() 
    {
        static DbConnectionPool instance;
        return instance;
    }

    // 初始化连接池
    void init(const std::string& host,
             const std::string& user,
             const std::string& password,
             const std::string& database,
             size_t poolSize = 10);

    // 获取连接
    std::shared_ptr<DbConnection> getConnection();

private:
    // 构造函数
    DbConnectionPool();
    // 析构函数
    ~DbConnectionPool();

    // 禁止拷贝
    DbConnectionPool(const DbConnectionPool&) = delete;
    DbConnectionPool& operator=(const DbConnectionPool&) = delete;

    std::shared_ptr<DbConnection> createConnection();

    void checkConnections(); // 添加连接检查方法

private:
    std::string                               host_;
    std::string                               user_;
    std::string                               password_;
    std::string                               database_;
    std::queue<std::shared_ptr<DbConnection>> connections_;  //使用队列存放连接池的连接
    std::mutex                                mutex_;
    std::condition_variable                   cv_; // 设置了10个连接，第11个线程来请求连接，当前没有，需要让线程等待，而不是返回空指针
    bool                                      initialized_ = false;
    std::thread                               checkThread_; // 添加检查线程  使连接池不仅管理连接而且维护连接健康
};

} // namespace db
} // namespace http

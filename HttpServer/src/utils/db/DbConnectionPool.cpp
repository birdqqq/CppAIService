#include "../../../include/utils/db/DbConnectionPool.h"
#include "../../../include/utils/db/DbException.h"
#include <muduo/base/Logging.h>

namespace http 
{
namespace db 
{

void DbConnectionPool::init(const std::string& host,
                          const std::string& user,
                          const std::string& password,
                          const std::string& database,
                          size_t poolSize) 
{
    // 连接池会被多个线程访问，所以操作其成员变量时需要加锁
    std::lock_guard<std::mutex> lock(mutex_);
    // 确保只初始化一次
    if (initialized_) 
    {
        return;
    }

    host_ = host;
    user_ = user;
    password_ = password;
    database_ = database;

    // 创建连接
    for (size_t i = 0; i < poolSize; ++i) 
    {
        // connections_是队列
        connections_.push(createConnection());
    }

    //创建完成，标志位置为true
    initialized_ = true;
    LOG_INFO << "Database connection pool initialized with " << poolSize << " connections";
}

DbConnectionPool::DbConnectionPool() 
{
    //创建检查线程，在连接池构造完成后不断检查连接状态
    checkThread_ = std::thread(&DbConnectionPool::checkConnections, this);
    //detach()作用，将checkthread_与管理它的操作系统线程分开
    checkThread_.detach();
}

DbConnectionPool::~DbConnectionPool() 
{
    std::lock_guard<std::mutex> lock(mutex_);
    while (!connections_.empty()) 
    {
        connections_.pop();
    }
    //connections_中存的是shared_ptr pop()之后当引用计数为0会自动调用析构执行自定义删除器，这就是RAII 见文档
    LOG_INFO << "Database connection pool destroyed";
}

// 修改获取连接的函数
std::shared_ptr<DbConnection> DbConnectionPool::getConnection() 
{
    std::shared_ptr<DbConnection> conn;
    {
        // 使用unique_lock是因为condition_variable要求必须是unique_lock锁
        //unique_lock在 wait()中会自动解锁 lock_guard不会
        std::unique_lock<std::mutex> lock(mutex_);
        
        //使用 while是因为线程会虚假唤醒 文档
        while (connections_.empty()) 
        {
            //剪枝 排除队列为空是因为没有初始化 这个原因
            if (!initialized_) 
            {
                throw DbException("Connection pool not initialized");
            }
            LOG_INFO << "Waiting for available connection...";
            //三件事：1.自动释放锁 2.当前线程睡眠 3.等待notify_one() 唤醒函数
            cv_.wait(lock);
        }
        
        conn = connections_.front();
        connections_.pop();
    } // 释放锁
    // 离开作用域 自动释放
    
    try 
    {
        // 在锁外检查连接
        // 查看连接状态：正常 不正常
        if (!conn->ping()) 
        {
            LOG_WARN << "Connection lost, attempting to reconnect...";
            conn->reconnect();
        }
        
        //确保业务代码一定拿到了可用的连接
        return std::shared_ptr<DbConnection>(conn.get(), 
            [this, conn](DbConnection*) {
                // 防止多线程同时访问临界区
                std::lock_guard<std::mutex> lock(mutex_);
                connections_.push(conn);
                // 通知一个正在等待的线程  对应上面的cv_.wait(lock)
                cv_.notify_one();
            // 自定义删除器 析构时不要delete 而是 将线程放进queue中，即还给连接池 见文档  RAII
            });
    } 
    catch (const std::exception& e) 
    {
        LOG_ERROR << "Failed to get connection: " << e.what();
        {
            // 就算发生异常，也将连接放回来连接池 保证连接池数量一致
            std::lock_guard<std::mutex> lock(mutex_);
            connections_.push(conn);
            cv_.notify_one();
        }
        throw;
    }
}

std::shared_ptr<DbConnection> DbConnectionPool::createConnection() 
{
    return std::make_shared<DbConnection>(host_, user_, password_, database_);
}

// 修改检查连接的函数
// 保活机制 防止连接池中的链接呢长时间不用而失效
void DbConnectionPool::checkConnections() 
{
    //写成死循环，不断检查数据库连接是否正常
    while (true) 
    {
        //将try catch放在while(true)中，因为不能一次报错就线程结束，需要不断地检查 返回日志。。。
        try 
        {
            // 作用：直接是connection_检查，加大锁的占用时间
            // 将链接复制一份，释放锁之后，慢慢检查
            std::vector<std::shared_ptr<DbConnection>> connsToCheck;
            {
                // 保护connentions_ 连接池这个队列
                std::unique_lock<std::mutex> lock(mutex_);
                // 所有连接都被使用，没有检查的必要，则等待一秒 开始下一轮循环 减少占用cpu时间
                if (connections_.empty()) 
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
                
                // 将connections_复制一份，因为queue没有迭代器，需要将连接放在vector中
                auto temp = connections_;
                while (!temp.empty()) 
                {
                    // 将connections_存进vector中
                    connsToCheck.push_back(temp.front());
                    temp.pop();
                }
            }
            
            // 在这里释放锁，因为后面需要ping()可能很慢 防止占用业务线程的进行
            // 锁只保护共享数据  不保护耗时操作
            // 真正地检查开始了
            // 在锁外检查连接
            for (auto& conn : connsToCheck) 
            {
                if (!conn->ping()) 
                {
                    // 这里放try catch 是因为 一个连接失败不能搁置后续连接的检查
                    try 
                    {
                        conn->reconnect();
                    } 
                    catch (const std::exception& e) 
                    {
                        LOG_ERROR << "Failed to reconnect: " << e.what();
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(60));
        } 
        catch (const std::exception& e) 
        {
            LOG_ERROR << "Error in check thread: " << e.what();
            // 假设整个数据库彻底挂了，等待5秒再进行检查 给程序一个缓冲期
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

} // namespace db
} // namespace http
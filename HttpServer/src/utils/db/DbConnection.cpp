#include "../../../include/utils/db/DbConnection.h"
#include "../../../include/utils/db/DbException.h"
#include <muduo/base/Logging.h>

namespace http 
{
namespace db 
{

DbConnection::DbConnection(const std::string& host,
                         const std::string& user,
                         const std::string& password,
                         const std::string& database)
    // 建立一条真正的MySQL数据库连接，完成初始化配置
    : host_(host)
    , user_(user)
    , password_(password)
    , database_(database)
{
    try 
    {
        // 获取MySQL驱动的单例对象 
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        // 连接数据库服务器，使用用户名密码验证 并交给shared_ptr管理
        conn_.reset(driver->connect(host_, user_, password_));  // 见文档
        if (conn_) 
        {
            conn_->setSchema(database_); // 明确接下来操作的数据库 是database_
            
            // 设置连接属性
            conn_->setClientOption("OPT_RECONNECT", "true"); // 断线自动重连开关
            conn_->setClientOption("OPT_CONNECT_TIMEOUT", "10"); // 连接建立超时时间 10s
            conn_->setClientOption("multi_statements", "false"); // 禁止在依次执行中传入多条SQL语句
            
            // 设置字符集
            std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
            stmt->execute("SET NAMES utf8mb4"); // 见文档
            
            LOG_INFO << "Database connection established"; // 打印连接建立成功的日志
        }
    } 
    catch (const sql::SQLException& e) 
    {
        LOG_ERROR << "Failed to create database connection: " << e.what();
        throw DbException(e.what());
    }
}

DbConnection::~DbConnection() 
{
    try 
    {
        cleanup();
    } 
    catch (...) 
    {
        // 析构函数中不抛出异常
    }
    LOG_INFO << "Database connection closed";
}

bool DbConnection::ping() 
{
    try 
    {
        // 不使用 getStmt，直接创建新的语句
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery("SELECT 1"));
        return true;
    } 
    catch (const sql::SQLException& e) 
    {
        LOG_ERROR << "Ping failed: " << e.what();
        return false;
    }
}

bool DbConnection::isValid() 
{
    try 
    {
        if (!conn_) return false;
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        stmt->execute("SELECT 1");
        return true;
    } 
    catch (const sql::SQLException&) 
    {
        return false;
    }
}

void DbConnection::reconnect() 
{
    try 
    {
        if (conn_) 
        {
            conn_->reconnect();
        } 
        else 
        {
            sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
            conn_.reset(driver->connect(host_, user_, password_));
            conn_->setSchema(database_);
        }
    } 
    catch (const sql::SQLException& e) 
    {
        LOG_ERROR << "Reconnect failed: " << e.what();
        throw DbException(e.what());
    }
}

void DbConnection::cleanup() 
{
    std::lock_guard<std::mutex> lock(mutex_);
    try 
    {
        if (conn_) 
        {
            // 确保所有事务都已完成
            if (!conn_->getAutoCommit()) 
            {
                conn_->rollback();
                conn_->setAutoCommit(true);
            }
            
            // 清理所有未处理的结果集
            std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
            while (stmt->getMoreResults()) 
            {
                auto result = stmt->getResultSet();
                while (result && result->next()) 
                {
                    // 消费所有结果
                }
            }
        }
    } 
    catch (const std::exception& e) 
    {
        LOG_WARN << "Error cleaning up connection: " << e.what();
        try 
        {
            reconnect();
        } 
        catch (...) 
        {
            // 忽略重连错误
        }
    }
}

} // namespace db
} // namespace http

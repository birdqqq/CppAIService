#pragma once
#include <memory>
#include <string>
#include <mutex>
#include <type_traits>
#include <cppconn/connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <mysql_driver.h>
#include <mysql/mysql.h>
#include <muduo/base/Logging.h>
#include "DbException.h"

namespace http 
{
namespace db 
{

class DbConnection 
{
public:
    DbConnection(const std::string& host, 
                const std::string& user,
                const std::string& password,
                const std::string& database);
    ~DbConnection();

    // 禁止拷贝
    // 数据库连接不能复制 同一时间只能有一个对象端粒一个数据库连接
    DbConnection(const DbConnection&) = delete;
    DbConnection& operator=(const DbConnection&) = delete;

    bool isValid();
    // 重连
    void reconnect();
    // 释放数据库资源
    void cleanup();

    // 作用给数据库执行一条安全的执行一条“代填参数的SQL语句”  SELECT
    template<typename... Args>
    sql::ResultSet* executeQuery(const std::string& sql, Args&&... args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        try 
        {
            // 直接创建新的预处理语句，不使用缓存
            std::unique_ptr<sql::PreparedStatement> stmt(
                conn_->prepareStatement(sql)
            );
            // 绑定参数
            bindParams(stmt.get(), 1, std::forward<Args>(args)...);
            // 返回执行结果result
            return stmt->executeQuery();
        } 
        catch (const sql::SQLException& e) 
        {
            LOG_ERROR << "Query failed: " << e.what() << ", SQL: " << sql;
            throw DbException(e.what());
        }
    }
    
    // 执行“会修改数据库内容”的SQL语句    UPDATE DELETE INSERT ...
    template<typename... Args>
    int executeUpdate(const std::string& sql, Args&&... args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        try 
        {
            // 直接创建新的预处理语句，不使用缓存
            std::unique_ptr<sql::PreparedStatement> stmt(
                conn_->prepareStatement(sql)
            );
            // 绑定参数
            bindParams(stmt.get(), 1, std::forward<Args>(args)...);
            // 返回int 表示影响/改动了多少行
            return stmt->executeUpdate();
        } 
        catch (const sql::SQLException& e) 
        {
            LOG_ERROR << "Update failed: " << e.what() << ", SQL: " << sql;
            throw DbException(e.what());
        }
    }

    bool ping();  // 添加检测连接是否有效的方法
private:
     // 辅助函数：递归终止条件
    // 当参数已经被绑定完了，调用这个重载结束递归
    void bindParams(sql::PreparedStatement*, int) {}
    
    // 辅助函数：绑定参数
    // 获取参数包中第一个参数
    // 注意：非 const 的 std::string 左值会命中本通用模板（精确匹配优先于 const 特化），
    // 因此必须用 if constexpr 区分算术类型(需要 to_string)与字符串(直接绑定)，
    // 否则对字符串调用 to_string 会编译失败。
    template<typename T, typename... Args>
    void bindParams(sql::PreparedStatement* stmt, int index,
                   T&& value, Args&&... args)
    {
        using U = std::decay_t<T>;
        if constexpr (std::is_arithmetic_v<U>)
        {
            // 算术类型：统一转成字符串，绑定到第 index 个占位符上
            stmt->setString(index, std::to_string(std::forward<T>(value)));
        }
        else
        {
            // 字符串类型：直接绑定到第 index 个占位符上（不能调用 to_string）
            stmt->setString(index, std::forward<T>(value));
        }
        // 递归 将剩下的参数传下去继续绑定
        bindParams(stmt, index + 1, std::forward<Args>(args)...);
    }

private:
    // 最重要的变量 见文档
    std::shared_ptr<sql::Connection> conn_;
    // ----------------------------------------------------------
    // 数据库配置
    std::string                      host_;
    std::string                      user_;
    std::string                      password_;
    std::string                      database_;
    // ----------------------------------------------------------
    // 锁，用于保护conn_区别于DbConnectionPool::mutex_保护connections_ queue
    std::mutex                       mutex_;
};

} // namespace db
} // namespace http
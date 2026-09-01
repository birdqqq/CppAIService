#pragma once

#include <SimpleAmqpClient/SimpleAmqpClient.h> // 成熟的 C++ RabbitMQ 客户端库，封装了 AMQP 协议的细节，提供了简单易用的接口
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>
#include <iostream>
#include <chrono>
#include <functional>

class MQManager {
// RabbitMQ生产者，管理RabbitMQ连接资源；提高发送消息接口；将消息发送到指定队列
public:
    // 获取单例实例  单例模式 全局只有一份连接池
    static MQManager& instance() {
        static MQManager mgr;
        return mgr;
    }

    // 发布消息到指定队列 两个参数：队列名称 和 要发布的消息内容
    void publish(const std::string& queue, const std::string& msg);

private:
    // MQConn 结构体表示一个 RabbitMQ 连接，包含一个 channel 和一个互斥锁，用于线程安全的消息发布
    struct MQConn {
        AmqpClient::Channel::ptr_t channel;
        std::mutex mtx;
    };

    // 构造 拷贝构造 赋值 私有化 public只提供获取实例化对象的接口  --->   单例模式
    MQManager(size_t poolSize = 5); // 构造函数

    MQManager(const MQManager&) = delete; // 禁止拷贝构造
    MQManager& operator=(const MQManager&) = delete; // 禁止赋值

    std::vector<std::shared_ptr<MQConn>> pool_; // 连接池本体
    size_t poolSize_;                           // 连接池大小
    std::atomic<size_t> counter_;               // 用于轮询选择连接
};

class RabbitMQThreadPool {
// RabbitMQ消费者 创建多个消费者线程；从RabbitMQ队列中消费消息；将消息交给回调函数处理
public:
    using HandlerFunc = std::function<void(const std::string&)>;

    RabbitMQThreadPool(const std::string& host,
        const std::string& queue,
        int thread_num,
        HandlerFunc handler)
        : stop_(false),
        rabbitmq_host_(host),
        queue_name_(queue),
        thread_num_(thread_num),
        handler_(handler) {}

    void start();
    void shutdown();

    ~RabbitMQThreadPool() {
        shutdown();
    }

private:
    void worker(int id);

private:
    std::vector<std::thread> workers_; // 存放工作线程对象
    std::atomic<bool> stop_;           // 停止信号标志位 通知所有works线程 该出系循环了 
    std::string queue_name_;           // 要消费的队列名称
    int thread_num_;                   // 启动消费线程的数量
    std::string rabbitmq_host_;        // RabbitMQ服务器地址
    HandlerFunc handler_;              // 消息处理回调函数
};

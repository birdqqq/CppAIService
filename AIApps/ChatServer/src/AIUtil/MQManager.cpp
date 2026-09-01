#include"../include/AIUtil/MQManager.h"

// ------------------- MQManager -------------------
MQManager::MQManager(size_t poolSize)
    : poolSize_(poolSize), counter_(0) {
    for (size_t i = 0; i < poolSize_; ++i) {
        auto conn = std::make_shared<MQConn>(); // 智能指针 计数 = 1
        //  Create
        // 这一步是真正发起网络连接的地方会经历 TCP 三次握手 + AMQP 协议握手 + 认证这一整套流程，如果 RabbitMQ 服务没启动或者账号密码不对，这里会抛出异常
        // host（服务器地址）、port（端口，5672 是 AMQP 协议默认端口）、username（用户名）、password（密码）、vhost（虚拟主机，"/"是默认虚拟主机）
        conn->channel = AmqpClient::Channel::Create("localhost", 5672, "guest", "guest", "/");

        pool_.push_back(conn); // 将创建好的channel放进pool_(一个vector)中 conn是shared_ptr此时引用计数+1 计数 = 2
    }
}
// 计数 = 1 因为离开函数 局部变量conn销毁 计数-1 --> =1；

// 已读不修改的参数使用 const& 
void MQManager::publish(const std::string& queue, const std::string& msg) {
    // fetch_add(1)原子性的将counter_+1,返回旧值！ 这样100个线程调用publish()得到的index也是不同的！
    // % poolSize_ 对连接池大小取模，把一个不断增长的计数值映射到 [0, poolSize_-1] 这个范围内，循环往复
    // 举例：poolSize_ = 5 时，counter_ 依次是 0,1,2,3,4,5,6,7... 取模后依次得到 index = 0,1,2,3,4,0,1,2...
    // 这就是轮询（round-robin）负载均衡算法在这里的具体实现：让每一次 publish 调用尽量均匀地分配到池子里的 5 条连接上,而不是老让某一条连接持续繁忙、其他连接空闲
    // 先fetch_add()得到全局唯一序号再进行取模  线程安全
    size_t index = counter_.fetch_add(1) % poolSize_; 
    auto& conn = pool_[index]; // 取出该线程 使用引用 避免复制shared_ptr，shared_ptr的复制会增加引用计数，降低性能

    std::lock_guard<std::mutex> lock(conn->mtx);
    auto message = AmqpClient::BasicMessage::Create(msg); // BasicMessage类是AMQP协议中消息的封装，提供了设置消息属性、获取消息内容等功能 ，消息内容只是其中的一个属性
    conn->channel->BasicPublish("", queue, message); // 真正发送消息的地方，调用 channel 的 BasicPublish 方法将消息发送到指定队列
    // “”表示默认交换机，RabbitMQ 中的交换机是消息路由的关键组件，默认交换机会将消息直接路由到与队列同名的队列上
    // queue 是队列名称，message 是要发送的消息对象 使用默认交换机时，队列名称必须与交换机路由键相同，才能确保消息被正确路由到目标队列
    // message 是 BasicMessage::ptr_t 类型的智能指针，指向一个 BasicMessage 对象，封装了消息的内容和属性
}

// ------------------- RabbitMQThreadPool -------------------

// 启动消费线程池
void RabbitMQThreadPool::start() {
    for (int i = 0; i < thread_num_; ++i) {
        workers_.emplace_back(&RabbitMQThreadPool::worker, this, i);// emplace_back()在容器末尾直接构造对象，避免了不必要的拷贝或移动操作，效率更高
        // ()中的参数是thread构造函数的参数,workers_是vector<thread>类型，存放工作线程对象
        // thread构造函数的参数是一个可调用对象（函数指针、函数对象、lambda表达式等）和该可调用对象的参数列表
        // worker是成员函数，成员函数指针需要一个对象实例才能调用，所以传入this指针作为第一个参数，i作为第二个参数
    }
}

void RabbitMQThreadPool::shutdown() {
    stop_ = true; // 设置停止标志位，通知所有工作线程退出循环
    // 带超时轮询优雅推出：不是立刻让所有线程停下来，只是"把信号旗子立起来"，各个 worker 线程要等到它们各自当前正在执行
    // 的这一轮 BasicConsumeMessage（最多500ms）结束，回到 while 循环开头重新判断条件时，才会真正感知到并退出
    for (auto& t : workers_) { // 遍历线程池中的每个线程
        if (t.joinable()) t.join(); // joinable()判断线程是否可连接，join()等待线程结束并回收资源，确保所有线程都已退出
        // 这里的 join()是阻塞的，直到线程执行完毕才会返回，确保所有线程都已退出，避免资源泄漏
    }
}

void RabbitMQThreadPool::worker(int id) {
    try {
        // 每个线程拥有各自独立的连接（channel）
        auto channel = AmqpClient::Channel::Create(rabbitmq_host_, 5672, "guest", "guest", "/");
        // 设置为非独占队列（exclusive=false，第三个参数是 durable=true）
        channel->DeclareQueue(queue_name_, false, true, false, false);
        // 防止出现 channel 报错：403: AMQP_BASIC_CONSUME_METHOD caused: ACCESS_REFUSED - queue 
        // 'sql_queue' in vhost '/' in exclusive use（队列被占用为独占模式导致拒绝访问）
        // std::string consumer_tag = channel->BasicConsume(queue_name_, "");
        std::string consumer_tag = channel->BasicConsume(queue_name_, "", true, false, false);

        channel->BasicQos(consumer_tag, 1); 

        while (!stop_) {
            AmqpClient::Envelope::ptr_t env;
            bool ok = channel->BasicConsumeMessage(consumer_tag, env, 500); // 500ms 
            if (ok && env) {
                std::string msg = env->Message()->Body();
                handler_(msg);          
                channel->BasicAck(env); 
            }
        }

        channel->BasicCancel(consumer_tag);
    }
    catch (const std::exception& e) {
        std::cerr << "Thread " << id << " exception: " << e.what() << std::endl;
    }
}

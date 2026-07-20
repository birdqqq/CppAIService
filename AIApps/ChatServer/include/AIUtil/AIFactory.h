#pragma once
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <sstream>
#include <memory>
#include <functional>
#include <unordered_map>
#include <string>


#include"AIStrategy.h"

class StrategyFactory {

public:
    // Creator是一个函数对象类型，返回一个没有参数的shared_ptr<AIStrategy>类型的函数，用于创建不同的AI策略实例
    using Creator = std::function<std::shared_ptr<AIStrategy>()>; 
    // 单例模式 只能有一个StrategyFactory实例，保证全局唯一性 所有策略只能注册到同一个工厂
    static StrategyFactory& instance();

    void registerStrategy(const std::string& name, Creator creator); // 注册工厂 

    std::shared_ptr<AIStrategy> create(const std::string& name);

private:
    // 单例模式 构造私有 静态接口 静态成员 见文档
    StrategyFactory() = default;
    // map中保存Creator 的原因：保存对象std::shared_ptr<AIStrategy>的话,程序启动，所有模型均完成创建，浪费内存，且不利于扩展
    // 使用Creator函数对象，只有在需要时才创建对应的AI策略实例，节省内存，提高性能 -----延迟创建(Lazy Create)！
    std::unordered_map<std::string, Creator> creators; 
};



// 注意：不要写成 static std::shared_ptr<AIStrategy> instance = std::make_shared<T>();
// 那样就是共享一个"单例"实例，每次从 map 里查到这个函数都会返回同一个实例

// 而上面这样写（没有 static），保证每次调用都会创建一个新的"实例"，保证线程安全

// 程序启动时自动注册策略
// 写 struct 而不是 class 是因为struct默认 public，方便 可以少写一个public:
template<typename T>
struct StrategyRegister {
    StrategyRegister(const std::string& name) {
        // instance()获取单例工厂对象factory,并且调用registerStrategy()将创建的策略注册到工厂中（creators）
        StrategyFactory::instance().registerStrategy(name, [] {
            std::shared_ptr<AIStrategy> instance = std::make_shared<T>(); // 这里的instance保存的是对应策略的构造
            });
    }
    // 此时registerStrategy中move到creators中的是lambda函数对象，lambda函数对象中保存了创建对应策略的构造函数，
    // 后续调用create()时就会调用这个lambda函数对象，从而创建对应策略实例
    // 实现 延迟构造！
};


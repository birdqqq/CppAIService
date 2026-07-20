#include"../include/AIUtil/AIFactory.h"


StrategyFactory& StrategyFactory::instance() {
    static StrategyFactory factory;
    return factory;
}

void StrategyFactory::registerStrategy(const std::string& name, Creator creator) {
    creators[name] = std::move(creator);
}

std::shared_ptr<AIStrategy> StrategyFactory::create(const std::string& name) {
    auto it = creators.find(name);
    if (it == creators.end()) {
        throw std::runtime_error("Unknown strategy: " + name);
    }
    return it->second(); // map的value是一个Creator函数对象，调用它就会返回一个新的AIStrategy实例 
                         //所以second() 这里是调用函数对象的操作符()，返回一个新的AIStrategy实例
}

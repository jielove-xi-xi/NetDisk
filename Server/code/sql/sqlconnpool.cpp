#include "sqlconnpool.h"


SqlConnPool *SqlConnPool::Instance()
{
    static SqlConnPool instance;
    return &instance;
}


// 初始化连接池
void SqlConnPool::Init(const std::string& host, const std::string& user, const std::string& password, 
                       const std::string& database, int connSize) 
{
    assert(connSize > 0);
    
    driver_ = get_driver_instance();  // 获取 MySQL 驱动
    
    std::lock_guard<std::mutex> lock(mtx_);
    for (int i = 0; i < connSize; ++i) {
        try {
            std::shared_ptr<sql::Connection> conn(driver_->connect(host, user, password));
            conn->setSchema(database);  // 设置数据库
            connQueue_.push(conn);      // 将连接放入队列
        } catch (sql::SQLException& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            throw std::runtime_error("Failed to create MySQL connection.");
        }
    }

    maxConn_ = connSize;
}

// 获取连接
std::shared_ptr<sql::Connection> SqlConnPool::GetConn() {
    std::unique_lock<std::mutex> lock(mtx_);

    // 如果连接池为空，等待
    cond_.wait(lock, [this]() { return !connQueue_.empty(); });

    // 从队列中取出连接
    auto conn = connQueue_.front();
    connQueue_.pop();

    return conn;
}

// 释放连接
void SqlConnPool::FreeConn(std::shared_ptr<sql::Connection> conn) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    // 将连接重新放回队列
    connQueue_.push(conn);
    cond_.notify_one();  // 通知等待线程
}

// 获取当前可用连接数
int SqlConnPool::GetFreeConnCount() {
    std::lock_guard<std::mutex> lock(mtx_);
    return connQueue_.size();
}

// 关闭连接池
void SqlConnPool::ClosePool() {
    std::lock_guard<std::mutex> lock(mtx_);
    
    // 清空连接队列
    while (!connQueue_.empty()) {
        connQueue_.pop();  // 使用 shared_ptr 自动管理资源，无需手动释放
    }
}

// 析构函数
SqlConnPool::~SqlConnPool() {
    ClosePool();
}

#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include<assert.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/connection.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>
#include<cppconn/prepared_statement.h>
#include"../function.h"

/*一个使用信号量和互斥量保证线程安全和线程通信的MYSQL连接池。可供复用，减少系统IO调用。
使用方法：先调用Instance()返回全局唯一实例，再通过唯一实例调用init()初始化，再调用GetConn获取一个连接，使用完后调用FreeConn将其放回连接池
最佳使用方:配合sqlconnRAII类使用，该类封装了自动释放连接，避免手动释放
*/
class SqlConnPool {
public:
    static SqlConnPool* Instance();                 // 获取单例实例

    SqlConnPool(const SqlConnPool&) = delete;       // 禁止拷贝构造和赋值
    SqlConnPool& operator=(const SqlConnPool&) = delete;

    void Init(const std::string& host, const std::string& user, const std::string& password, 
              const std::string& database, int connSize = 10);          // 初始化连接池

    std::shared_ptr<sql::Connection> GetConn();                  // 获取一个连接
    void FreeConn(std::shared_ptr<sql::Connection> conn);       // 释放连接
    int GetFreeConnCount();                                     // 获取当前可用连接数
    void ClosePool();                                           // 关闭连接池

private:
    // 私有构造函数，防止外部创建实例
    SqlConnPool() = default;
    ~SqlConnPool();

    int maxConn_;                                  // 最大连接数
    std::queue<std::shared_ptr<sql::Connection>> connQueue_;  // 连接队列
    std::mutex mtx_;                               // 互斥锁
    std::condition_variable cond_;                 // 条件变量，用于线程间通信
    sql::Driver* driver_;                          // MySQL 驱动
};


#endif

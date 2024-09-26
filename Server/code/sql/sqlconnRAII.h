#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.h"

/* MYSQL连接池的RAII类，可以在构造时候申请连接对象，离开作用域自动放回连接池
资源在对象构造初始化 资源在对象析构时释放
*/
class SqlConnRAII {
public:
    // 构造函数，传递一个连接对象的共享指针和连接池指针
    SqlConnRAII(std::shared_ptr<sql::Connection>& sql, SqlConnPool *connpool) {
        assert(connpool);
        sql = connpool->GetConn();
        connpool_ = connpool;
        sql_ = sql;
    }

    // 自动将连接放回连接池
    ~SqlConnRAII() {
        if (sql_) { connpool_->FreeConn(sql_); }
    }

private:
    std::shared_ptr<sql::Connection> sql_;
    SqlConnPool* connpool_;
};

#endif //SQLCONNRAII_H
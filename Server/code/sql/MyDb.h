#ifndef MYDB_H
#define MYDB_H
#include"sqlconnRAII.h"
#include <iostream>
#include <string>
#include <cassert>
#include <sstream>
#include <string.h>
#include <sstream>
#include <iomanip>
#include"../function.h"
#include"../pool/UpDownCon.h"
using namespace std;

//用于简化Mysql查询处理类
class MyDb 
{
private:
    static std::string tablename;    // 表名
    static std::string Columns1;     // 字段1
    static std::string Columns2;     // 字段2
public:
    MyDb();
    ~MyDb();

    // 查询接口
    bool getUserInfo(const std::string &username, const std::string &password, UserInfo &info);             //查询用户信息
    bool getUserExist(const std::string &username);                                                         //查询用户是否存在
    bool insertUser(const std::string &username, const std::string &password, const std::string &cipher);   //插入用户
    bool GetFileMd5(const std::string &user, const std::uint64_t &fileid,string &md5);                      //获取文件MD5
    bool GetISenoughSpace(const std::string &username, const std::string &password,std::uint64_t filesize); //检查空间是否足够
    bool getFileExist(const std::string &username,const std::string &MD5);                                  //查询是否已经存在该文件，支不支持妙传
    bool getUserAllFileinfo(const std::string &username,vector<FILEINFO>&vet);                              //获取用户在数据库中的全部文件信息

    std::uint64_t insertFiledata(const string &user,UDtask *task,const string &suffix);                     //插入文件数据
    bool deleteOneFile(const string &user,std::uint64_t &fileid);                                           //删除单个文件
    bool deleteOneDir(const string &user,std::uint64_t &fileid);                                            //删除文件夹
private:
    bool executeSelect(const std::string &sql, const std::vector<std::string> &params, std::vector<std::string>& result);
    bool executealter(const std::string &sql, const std::vector<std::string> &params);
    bool executeSelect(const std::string &sql, const std::vector<std::string> &params, std::vector<vector<string>>& result);
    std::shared_ptr<sql::Connection> conn;
    SqlConnRAII* connRAII = nullptr;
};


#endif




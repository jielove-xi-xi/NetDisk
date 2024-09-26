#include "MyDb.h"
#include <iostream>



std::string MyDb::tablename = "Users";
std::string MyDb::Columns1 = "User";
std::string MyDb::Columns2 = "Password";

MyDb::MyDb() {
    connRAII = new SqlConnRAII(conn, SqlConnPool::Instance());  // 从连接池获取连接
}

MyDb::~MyDb() {
    if (connRAII) {
        delete connRAII;
    }
}


//传递用户名和密码，从数据库返回用户信息，保存在结果info
bool MyDb::getUserInfo(const std::string &username, const std::string &password, UserInfo &info) {
    std::string sql = "SELECT * FROM " + tablename + " WHERE " + Columns1 + "=? AND " + Columns2 + "=? LIMIT 1";
    std::vector<std::string> params = {username, password};
    std::vector<std::string> result;
    executeSelect(sql, params, result);  // 根据查询结果更新info
    return ConvertTouserInfo(result,info);
}

//返回用户是否存在，存在返回false,不存在返回true,则可以进行注册操作
bool MyDb::getUserExist(const std::string &username) {
    std::string sql = "SELECT User FROM " + tablename + " WHERE " + Columns1 + " = ?";
    std::vector<std::string> params = {username};
    std::vector<std::string> result;
    executeSelect(sql, params, result);

    return result.empty();
}

//传递用户名，密码，哈希密文创建用户
bool MyDb::insertUser(const std::string &username, const std::string &password, const std::string &cipher) {
    std::string sql = "INSERT INTO Users (User, Password, Cipher) VALUES (?, ?, ?)";
    std::vector<std::string> params = {username, password, cipher};
    return executealter(sql, params);
}

//传递所属用户名、文件后缀名、MD5、该文件所属父目录ID，插入该文件。不存在父目录，插入根目录，默认为0.返回插入数据库中文件的ID
std::uint64_t MyDb::insertFiledata(const string &user,UDtask *task,const string &suffix) {
    //1、检查父ID是否存在
    string sql="select DirGrade FROM FileDir where User=? and Fileid = ? and FileType ='d'";    
    vector<string>params;
    vector<string>ret;
    params.push_back(user);
    params.push_back(to_string(task->parentDirID));

    int DirGrade=0; //文件夹距离根文件夹距离。
    if(!executeSelect(sql,params,ret)) //不存在该文件夹，将其保存在根文件夹，如果存在，返回文件夹等级
        task->parentDirID=0;
    else
        DirGrade=stoi(ret[0])+1;        //等级为父文件夹等级+1


    //2、查询文件当前最大ID
    sql="SELECT Fileid FROM FileDir WHERE User=? ORDER BY Fileid DESC LIMIT 1";
    params.resize(1);
    params[0]=user;
    ret.clear();
    uint64_t fileid=0;
    if(executeSelect(sql,params,ret))
        fileid=std::stoull(ret[0])+1;   //当前最大ID+1
    else
        fileid=1;  //初始化为1


    //3、插入文件信息到文件表
    sql="INSERT INTO FileDir (Fileid,User,FileName,DirGrade,FileType,MD5,FileSize,ParentDir) VALUES(?,?,?,?,?,?,?,?)";
    params.clear();
    params.push_back(to_string(fileid));
    params.push_back(user);
    params.push_back(task->Filename);
    params.push_back(to_string(DirGrade));
    params.push_back(suffix);      //d是文件夹，其它文件夹
    params.push_back(task->FileMD5);
    params.push_back(to_string(task->total));
    params.push_back(to_string(task->parentDirID));

    if(!executealter(sql,params))
    {
        perror("Insert FileDir database failed");
        return false;
    }


    params.clear();
    //4、更新已用空间
    sql="UPDATE Users SET usedCapacity=usedCapacity+? where User=?";
    params.push_back(to_string(task->total));
    params.push_back(string(user));

    if(!executealter(sql,params))
    {
        perror("Updata FileDir database failed");
        return false;
    }

    return fileid;
}

//删除单个文件
bool MyDb::deleteOneFile(const string &user, std::uint64_t &fileid)
{
    if (fileid == 0) return false;

    // 先保存该文件的MD5码
    string MD5;
    if (!GetFileMd5(user, fileid, MD5))  // 文件不存在，返回false
        return false;

    // 查找相同MD5在数据库中的数量
    string sql = "SELECT * FROM FileDir WHERE User=? AND MD5=?";
    vector<string> parms = { user, MD5 };
    vector<vector<string>> file_count;

    if (!executeSelect(sql, parms, file_count))
        return false;

    sql = "DELETE FROM FileDir WHERE Fileid=? AND User=? AND MD5=?";
    parms = { to_string(fileid), user, MD5 };

    bool success = executealter(sql, parms);
    if (!success) return false;

    if (file_count.size() == 1) 
    {
        string filepath = string(ROOTFILEPATH)+"/"+user + "/" + MD5;
        int fd = open(filepath.c_str(), O_RDONLY);  //尝试以独占打开文件，如果失败说明正在使用，不能删除
        if (fd != -1) 
        {
            close(fd);
            if (remove(filepath.c_str()) == 0)
                return true;
        }
        std::cerr << "Error deleting file: " << filepath << std::endl;
        return false;  
    }
    return true;
}

//递归删除一个文件夹。性能影响较大，可以采用数据库存储过程等优化
bool MyDb::deleteOneDir(const string &user, std::uint64_t &fileid)
{
    if (fileid == 0) return false;  // 检查 fileid 是否有效
    
    // 第一步，查询该文件的所有子项
    string sql = "SELECT Fileid, FileType FROM FileDir WHERE User=? AND ParentDir=?";
    vector<string> params{user, to_string(fileid)};
    vector<vector<string>> ret;

    executeSelect(sql, params, ret);


    // 递归删除子项
    for (const auto &row : ret) {
        uint64_t curfileId = std::stoull(row[0]);
        if (row[1] == "d") {  // 如果是文件夹，递归删除
            if (!deleteOneDir(user, curfileId)) {
                std::cerr << "Failed to delete subdirectory: " << curfileId << std::endl;
                return false;
            }
        } else {
            if (!deleteOneFile(user, curfileId)) {
                std::cerr << "Failed to delete file: " << curfileId << std::endl;
                return false;
            }
        }
    }

    // 最后删除本目录
    sql = "DELETE FROM FileDir WHERE Fileid=? AND User=?";
    params = {to_string(fileid), user};

    if (!executealter(sql, params)) {
        std::cerr << "Failed to delete directory: " << fileid << std::endl;
        return false;
    }

    return true;  // 成功删除
}

//传递用户名，文件id，获取MD5码保存在md5中，成功返回true.
bool MyDb::GetFileMd5(const std::string &user, const std::uint64_t &fileid,string &md5)
{
    string sql="SELECT MD5 FROM FileDir WHERE Fileid=? AND User=? AND FileType!='d'"; //文件夹无法下载，没有MD5码。所以排除文件夹
    vector<string>parmas(2);
    parmas[0]=to_string(fileid);
    parmas[1]=user;

    vector<string>ret;
    if(!executeSelect(sql,parmas,ret))
        return false;
    if(ret.empty())
        return false;
    md5=ret[0];
    return true;
}

// 检查空间是否足够
bool MyDb::GetISenoughSpace(const std::string &username, const std::string &password, std::uint64_t filesize)
{
    // 查询用户的总容量和已使用容量
    std::string sql = "SELECT CapacitySum, usedCapacity FROM Users WHERE User=? AND Password=?";
    
    // 参数化查询防止 SQL 注入
    std::vector<std::string> parmas(2);
    parmas[0] = username;
    parmas[1] = password;

    // 存储查询结果
    std::vector<std::string> ret;
    executeSelect(sql, parmas, ret);

    // 如果查询结果为空，表示用户不存在或密码错误
    if (ret.empty())
        return false;

    try {
        // 转换容量值为整数类型
        std::uint64_t totalCapacity = std::stoull(ret[0]);
        std::uint64_t usedCapacity = std::stoull(ret[1]);

        // 计算剩余容量是否足够
        return (totalCapacity - usedCapacity) >= filesize;
    } catch (const std::invalid_argument &e) {
        // 捕捉无效的转换异常，比如非数字字符
        std::cerr << "Invalid argument: " << e.what() << std::endl;
        return false;
    } catch (const std::out_of_range &e) {
        // 捕捉转换时溢出的异常
        std::cerr << "Out of range: " << e.what() << std::endl;
        return false;
    }
}

// 查询是否存在该文件，是否支持妙传.如果存在返回真，否则返回false
bool MyDb::getFileExist(const std::string &username, const std::string &MD5)
{
    string sql="select Fileid from FileDir where User=? and MD5=?";
    vector<string>params(2);
    params[0]=username;
    params[1]=MD5;

    vector<string>result;
    executeSelect(sql,params,result);
    return !result.empty();
}

//传递用户名，和保存返回结果的文件信息结构体容器，返回用户在数据库中的文件信息。
bool MyDb::getUserAllFileinfo(const std::string &username, vector<FILEINFO> &vet)
{
    std::string sql="select Fileid,FileName,DirGrade,FileType,FileSize,ParentDir,FileDate from FileDir where User=?";
    std::vector<std::string> params(1,username);
    std::vector<std::vector<std::string>>ret;


    if(!executeSelect(sql,params,ret))
    {
        return false;           //如果没有数据，返回false
    }

    vet.reserve(ret.size());

    //将返回结果保存到vet
    for(size_t i=0;i<ret.size();++i)
    {
        FILEINFO fileinfo;
        if(VetToFileInfo(ret[i],fileinfo))
        {
            vet.emplace_back(fileinfo);
        }
        else
            return false;
    }
    return true;
}

//通用查询，传递sql指令和参数，将查询结果保存在result,只返回一条数据
bool MyDb::executeSelect(const std::string &sql, const std::vector<std::string> &params, std::vector<std::string>& result) {
    try {
        std::unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(sql));
        for (size_t i = 0; i < params.size(); ++i) {
            stmt->setString(i + 1, params[i]);
        }

        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            for (size_t i = 1; i <= res->getMetaData()->getColumnCount(); ++i) {
                result.push_back(res->getString(i));
            }
            break;
        }
        return !result.empty();
    } catch (sql::SQLException &e) {
        std::cerr << "SQL error: " << e.what() << std::endl;
        return false;
    }
}

//通用查询，传递sql指令和参数，将查询结果保存在result,返回多条数据
bool MyDb::executeSelect(const std::string &sql, const std::vector<std::string> &params, std::vector<vector<string>>& result)
{
    try {
        std::unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(sql));
        for (size_t i = 0; i < params.size(); ++i) {
            stmt->setString(i + 1, params[i]);
        }

        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());

        size_t columnCount=res->getMetaData()->getColumnCount();    //获取列数

        //遍历结果
        while(res->next())
        {
            std::vector<std::string>row;
            for(size_t j=1;j<=columnCount;++j)
                row.push_back(res->getString(j));
            result.push_back(row);
        }
        return !result.empty(); //返回是否有数据
    } catch (sql::SQLException &e) {
        std::cerr << "SQL error: " << e.what() << std::endl;
        return false;
    }
}


//通用更新和插入，传递sql指令，和参数。返回是否成功。
bool MyDb::executealter(const std::string &sql, const std::vector<std::string> &params) {
    try {
        std::unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(sql));
        for (size_t i = 0; i < params.size(); ++i) {
            stmt->setString(i + 1, params[i]);
        }

        stmt->execute();
        return true;
    } catch (sql::SQLException &e) {
        std::cerr << "SQL error: " << e.what() << std::endl;
        return false;
    }
}
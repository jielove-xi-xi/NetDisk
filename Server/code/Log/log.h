#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // vastart va_end
#include <assert.h>
#include <sys/stat.h>         //mkdir
#include "blockqueue.h"
#include "../buffer/buffer.h"


/*
Log类是个支持同步或者异步的日志系统，使用单例模式进行构建。保证全局只有唯一实例
将日志系统分为三个级别：0：debug、1：info、2：warn、3：error，可用调用SetLevel设置记录的日志最低级别，如级别2，则0和1不会记录，提高系统性能
使用方法：先调用Instance()返回唯一实例，然后通过实例调用init初始化，最后即可调用四个宏定义写入日记
*/
class Log {
public:
    void init(int level, const char* path = "./logfile", const char* suffix =".log",int maxQueueCapacity = 1024);

    static Log* Instance();         //获取日记类的唯一单例
    static void FlushLogThread();   //刷新日志线程，使线程异步写入日志,用于传递给线程的运行函数，构造线程

    void write(int level, const char *format,...);  //传递日志级别（0:debug、1:info、2:warn、3：error），格式化字符串，将该字符串写入日记文件
    void flush();                   //将文件指针中的数据写入系统内核，存入文件

    int GetLevel();                 //获取当前日志级别，0:debug、1:info、2:warn、3：error，低于这个级别的日记不会被写入，如为3，怎0 1 2 都不会被记录
    void SetLevel(int level);       //设置当前日志级别，0:debug、1:info、2:warn、3：error
    
    //日志系统是否开始
    bool IsOpen() { return isOpen_; }   
    
private:
    Log();                          //构造函数，初始化成员变量，默认为同步写入
    void AppendLogLevelTitle_(int level);       //析构函数
    virtual ~Log();
    void AsyncWrite_();                     //异步写入日记

private:
    static const int LOG_PATH_LEN = 256;    //日志保存路径最大长度
    static const int LOG_NAME_LEN = 256;    //日志名最大长度
    static const int MAX_LINES = 50000;     //单个日志文件记录最大行数

    const char* path_;      //日志文件路径
    const char* suffix_;    //日志文件后缀名

    int MAX_LINES_;         //日志文件最大行数

    int lineCount_;         //当前日志文件行数
    int toDay_;             //当前日志文件日期

    bool isOpen_;           //是否开始日志
 
    Buffer buff_;           //日志缓存区
    int level_;             //日志级别
    bool isAsync_;          //是否异步写入，1则为异步

    FILE* fp_;              //日志文件指针
    std::unique_ptr<BlockDeque<std::string>> deque_;    //阻塞队列智能指针，内部存储string
    std::unique_ptr<std::thread> writeThread_;          //异步写入日志线程智能指针
    std::mutex mtx_;                                    //互斥锁
};


//统一调用写入日志宏定义，输入日记等级（0:debug、1:info、2:warn、3：error），和格式化字符串，写入日记
#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H
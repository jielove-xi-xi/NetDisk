#include "log.h"

using namespace std;

//构造函数，初始化成员变量，默认为同步写入
Log::Log() {
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

//析构函数
Log::~Log() {

    //这可以防止错误地调用 join()，因为如果线程对象已经被联结或分离，再调用 join() 会导致未定义行为。
    if(writeThread_ && writeThread_->joinable()) {
        while(!deque_->empty()) {   //等待处理完全部日志后，关闭日志系统，等待线程结束
            deque_->flush();
        };
        deque_->Close();
        writeThread_->join();
    }
    if(fp_) {           //保证同步情况下，关闭前写入日志
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

//获取当前日志级别，0:debug、1:info、2:warn、3：error
int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_);
    return level_;
}

//设置当前日志级别，0:debug、1:info、2:warn、3：error
void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}

//传递参数：level(日志等级，默认为0)，path(日志保存路径)，suffix(日志文件后缀名)，
//maxQueueSize(日志队列最大容量,决定是异步还是同步，为0则同步)初始化日志系统
void Log::init(int level = 0, const char* path, const char* suffix,int maxQueueSize) {
    isOpen_ = true; //默认开启日志
    level_ = level; //初始化等级
    if(maxQueueSize > 0) {  //如果队列长度大于0，则异步记录日志
        isAsync_ = true;
        if(!deque_) {   //如果队列指针为空，构造一个新队列对象和线程对象，并将其移动到成员变量
            unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            deque_ = move(newDeque);
            
            std::unique_ptr<std::thread> NewThread(new thread(FlushLogThread));
            writeThread_ = move(NewThread);
        }
    } else {    //关闭异步
        isAsync_ = false;
    }

    lineCount_ = 0;     //当前记录行数为0

    //获取系统当前时间
    time_t timer = time(nullptr);   
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;

    //设置日志文件路径和后缀名
    path_ = path;
    suffix_ = suffix;

    //创建日记文件名
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_); //格式化输入字符，将格式化字符数据，写入fileName
    toDay_ = t.tm_mday;     //记录日志文件创建日期

    {
        lock_guard<mutex> locker(mtx_);     //清空缓冲区，如果fp_已经打开，并且有数据，将其刷新进入系统文件，并关闭
        buff_.RetrieveAll();
        if(fp_) { 
            flush();
            fclose(fp_); 
        }

        //重新打开文件，以追加写的方式打开
        fp_ = fopen(fileName, "a");
        if(fp_ == nullptr) {        //如果文件打开失败，创建目录，并重试0777 是权限位，表示目录的权限设置为读写执行（对所有用户）
            mkdir(path_, 0777);
            fp_ = fopen(fileName, "a");
        } 
        assert(fp_ != nullptr);
    }
}

//传递日志级别（0:debug、1:info、2:warn、3：error），格式化字符串，将该字符串写入日记文件
void Log::write(int level, const char *format, ...) {
    //获取当前时间
    struct timeval now = {0, 0};    //定义时间变量
    gettimeofday(&now, nullptr);    //获取当前时间
    time_t tSec = now.tv_sec;       //转换为时间戳
    struct tm *sysTime = localtime(&tSec);  //转换为本地时间
    struct tm t = *sysTime;     //拷贝到新成员，为了保证线程安全，应该这么做

    //可变参数列表
    va_list vaList;

    //检查是否需要切换日志文件，如果时间已经是第二天，或者当前行数已达到最大行数
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_  %  MAX_LINES == 0)))
    {
        unique_lock<mutex> locker(mtx_);
        locker.unlock();
        
        //构造新文件
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (toDay_ != t.tm_mday)
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }
        else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }
        
        //上锁，并将现有缓存写入文件，然后重新创建新文件，并让fp_指针指向新文件
        locker.lock();
        flush();
        fclose(fp_);
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }
    //将日志记录写入缓冲区
    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        //将时间写入缓冲区
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
                 
        buff_.HasWritten(n);    //标志已经写入n字节
        AppendLogLevelTitle_(level);    //将等级信息写入缓冲区

        //将格式化字符串写入缓冲区
        va_start(vaList, format);
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);    //将换行符和结束符写入缓冲区


        //根据是异步还是同步，则异步写入，否则直接同步写入
        if(isAsync_ && deque_ && !deque_->full()) {
            deque_->push_back(buff_.RetrieveAllToStr());
        } else {
            fputs(buff_.Peek(), fp_);   //fputs将一个以\0结尾的字符串写入文件
        }
        buff_.RetrieveAll();    //清空缓存区
    }
}

//将日志记录的等级添加进入缓冲区，0:debug、1:info、2:warn、3：error,默认info
void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}

//将文件指针中的数据写入系统内核，存入文件
void Log::flush() {
    if(isAsync_) { 
        deque_->flush(); 
    }
    fflush(fp_);
}

//异步写入日记
void Log::AsyncWrite_() {
    //循环的从阻塞队列中读取日志数据到str,然后写入fp_
    string str = "";
    while(deque_->pop(str)) {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

//获取日记类的唯一单例
Log* Log::Instance() {
    static Log inst;
    return &inst;
}

//刷新日志线程，使线程异步写入日志,用于传递给线程的运行函数，构造线程
void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite_();
}
#ifndef BUFFER_H
#define BUFFER_H
#include <cstring>   
#include <iostream>
#include <unistd.h>  
#include <sys/uio.h> 
#include <vector> 
#include <atomic>
#include <assert.h>

/*
c++实现的智能缓存区，内部使用std::atomic实现线程读取安全，若写入数据安全做法需要借助互斥锁保证安全。使用方法
一、使用其它函数往缓冲区读取或者写入数据
    1、读取：先调用Peek返回读取的起始位置，再调用ReadableBytes()返回可供读取字节数。然后读取数据，自由选择读取长度len，再调用Retrieve标志已经读取len字节数据
    2、写入：先调用BeginWrite()返回写入位置指针，再调用WritableBytes()返回可写入字节数，然后往缓冲区自由写入len字节数，
    然后调用HasWritten(size_t len)标志写入多少字节，如果空间不够调用EnsureWriteable(size_t len)增加空间，增加空间后需要重新返回写入位置。
二、调用Append直接加入缓冲区（有四个重载版本），函数内部会自动拓展空间和更改位置指针
三、往文件字柄读取数据和写入数据，调用ReadFd(int fd, int* Errno);和WriteFd(int fd, int* Errno);
*/
class Buffer {
public:
    Buffer(int initBuffSize = 1024);
    ~Buffer() = default;

    //属性函数，用于返回类的相关属性

    size_t WritableBytes() const;       //返回可用来写入的字节数
    size_t ReadableBytes() const ;      //返回缓存区可供读取字节数
    size_t PrependableBytes() const;    //返回读区指针的位置，也可用作为预留空间位置，因为写指针前面的空间已经读取完了，代表可用重新使用

    const char* Peek() const;           //返回当前可读数据指针，若程序进行读取，重这里开始读取
    const char* BeginWriteConst() const;//返回缓存区写入位置指针，const版本，只读，不可进行写操作
    char* BeginWrite();                 //返回缓存区写入位置指针，可以进行写操作，即写入数据从这里开始写入
    void EnsureWriteable(size_t len);   //检查缓冲区是否还有len字节的可用空间，如果没有则增加
    

    //用于修改类的相关属性

    void HasWritten(size_t len);            //标志已经写入len字节数据，并让写坐标增加len字节
    void Retrieve(size_t len);              //标志已经读取了多少字节
    void RetrieveUntil(const char* end);    //将缓存区已经读取位置标志到end
    void RetrieveAll() ;                    //清空缓冲区，将readPos和writePos置0
    std::string RetrieveAllToStr();         //将缓存区的全部数据转为string来返回,并清空

    //将数据追加到缓冲区
    void Append(const std::string& str);        //追加写入一个string字符串进缓冲区
    void Append(const char* str, size_t len);   //将str字符数组的len字节数据写入缓存区
    void Append(const void* data, size_t len);  //将data void数组转换成字符数组，并将其中len字节的数据追加写入缓冲区
    void Append(const Buffer& buff);            //将其它Buffer对象的数据拷贝到缓冲区，从读取位置开始的全部字节

    ssize_t ReadFd(int fd, int* Errno);         //将套接字fd字柄的数据读入缓冲区，若出错则更新saveErrno,以提醒其它工作流程，并返回读取的字节数。
    ssize_t WriteFd(int fd, int* Errno);        //将缓冲区的数据可供读取的数据写入套接字fd,如果出错更改saveErrno以提醒其它工作流程，返回写入的字节长度

private:
    char* BeginPtr_();                  //返回缓存区起始地址 非const版本
    const char* BeginPtr_() const;      //返回缓存区起始地址 const版本
    void MakeSpace_(size_t len);        //缓存区不足后，拓展len字节空间


    std::vector<char> buffer_;          //内部缓冲区，用vector实现
    std::atomic<std::size_t> readPos_;  //用于确保多线程安全操作的类，内部保存一个size_t对象，指向缓冲区读取下标位置
    std::atomic<std::size_t> writePos_; //用于确保多线程安全操作的类，内部保存一个size_t对象，指向缓冲区写入下标位置
};

#endif //BUFFER_H
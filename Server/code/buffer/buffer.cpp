#include "buffer.h"

//初始化缓存区大小为initbuffsize,读位置和写位置为0
Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

//返回缓存区可供读取字节数
size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;    //写位置-读位置即为可供读取字节数
}

//返回可用来写入的字节数
size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;  //总长度-写下标即为可用来写字节数
}

//返回读区指针的位置，也可用作为预留空间位置，因为写指针前面的空间已经读取完了，代表可用重新使用
size_t Buffer::PrependableBytes() const {
    return readPos_;
}

//返回当前可读数据指针，若程序进行读取，重这里开始读取
const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;  //缓冲区起始地址加上读指针偏移量
}

//标志已经读取了多少字节
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes()); //读取字节必须小于可供读取字节数
    readPos_ += len;        //读指针增加
}

//将缓存区已经读取位置标志到end
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek());
}

//清空缓冲区，将readPos和writePos置0
void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size()); //将缓冲区的全部数据置0
    readPos_ = 0;
    writePos_ = 0;
}

//将缓存区的全部数据转为string来返回,并清空
std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

//返回缓存区写入位置指针，const版本，只读，不可进行写操作
const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

//返回缓存区写入位置指针，可以进行写操作，即写入数据从这里开始写入
char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

//标志已经写入len字节数据，并让写坐标增加len字节
void Buffer::HasWritten(size_t len) {
    writePos_ += len;
} 

//追加写入一个string字符串进缓冲区
void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());   //调用另外一条函数
}

//将data void数组转换成字符数组，并将其中len字节的数据追加写入缓冲区
void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);    //转换为char*指针
}

//将str字符数组的len字节数据写入缓存区
void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len);   //确保有足够的空间
    std::copy(str, str + len, BeginWrite());    //将str到str+len位置的数据从BeginWrite返回的写入位置开始写入缓冲区
    HasWritten(len);
}

//将其它Buffer对象的数据拷贝到缓冲区，从读取位置开始的全部字节
void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

//检查缓冲区是否还有len字节的可用空间
void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) {     //如果空间不够，增加len字节空间
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len); //确保可用空间大于等于len字节
}

//将套接字fd字柄的数据读入缓冲区，若出错则更新saveErrno,以提醒其它工作流程，并返回读取的字节数。
ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char buff[65535];
    struct iovec iov[2];            //即IO vector Unix/Linux 系统中进行 Scatter-Gather I/O 操作的结构体
    const size_t writable = WritableBytes();    //返回缓冲区剩余可使用来写入的字节数
    /* 分散读， 保证数据全部读完 */
    iov[0].iov_base = BeginPtr_() + writePos_;  //将iov的起始地址指向缓冲区的写入坐标
    iov[0].iov_len = writable;  //将iov的长度设置为缓冲区剩余可用空间
    iov[1].iov_base = buff;     //指向临时缓冲区的首地址
    iov[1].iov_len = sizeof(buff);  //长度为buff长度，即65535

    const ssize_t len = readv(fd, iov, 2);  //将套接字的数据写入iov,先写入第一个，第一个满了，再写入第二个
    if(len < 0) {                           //如果len=0,说明出现错误，将saveErron设置为errno,以通知其它工作流程
        *saveErrno = errno;
    }                                       //如果第一个iov可用存下这些数据，则直接修改缓冲区写索引位置
    else if(static_cast<size_t>(len) <= writable) {         
        writePos_ += len;
    }
    else {                                  //如果第一个iov空间不够，将写索引改成缓冲区长度，最后的位置，并调用Append追加入缓冲区，内部会扩展空间
        writePos_ = buffer_.size();
        Append(buff, len - writable);
    }
    return len;
}

//将缓冲区的数据可供读取的数据写入套接字fd,如果出错更改saveErrno以提醒其它工作流程，返回写入的字节长度
ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();      //返回可供读取字节数
    ssize_t len = write(fd, Peek(), readSize);  //往套接字写入数据
    if(len < 0) {           //如果等于0，则出错
        *saveErrno = errno;
        return len;
    } 
    readPos_ += len;    //修改读取位置索引
    return len;         //返回写入长度
}

//返回缓存区起始地址 非const版本
char* Buffer::BeginPtr_() {
    return &*buffer_.begin();   //返回迭代器，在解引用成char,再&取地址返回指针
}

//返回缓存区起始地址 const版本
const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}

//缓存区不足后，拓展len字节空间
void Buffer::MakeSpace_(size_t len) {
    if(WritableBytes() + PrependableBytes() < len) {    //可供读取字节数+预览留可重用字节数如果小于len,则重新拓展缓冲区
        buffer_.resize(writePos_ + len + 1);
    } 
    else {                                              //如果大于len,
        size_t readable = ReadableBytes();              //返回调整可供读取字节数
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());    //将还没读取的数据拷贝到缓冲区前面
        readPos_ = 0;               //读取位置标为0
        writePos_ = readPos_ + readable;    //更改读索引
        assert(readable == ReadableBytes());    //确保正常
    }
}
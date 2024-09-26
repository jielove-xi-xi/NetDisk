#include "DiskClient.h"
#include <QMetaType>
#include <QtWidgets/QApplication>
#include"function.h"

int main(int argc, char *argv[])
{
    
    QApplication a(argc, argv);
    qRegisterMetaType<FILEINFO>("FILEINFO");    //将FILEINFO注册到元对象系统，方法QVariant使用
    qRegisterMetaType<ItemDate>("ItemDate");

    DiskClient client;
    if (client.startClient()==true)
    {   
        client.show();
        return a.exec();
    }
    else
        return 0;
    return a.exec();
}

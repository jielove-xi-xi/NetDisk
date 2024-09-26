#ifndef ABSTRACTTOOL_H
#define ABSTRACTTOOL_H

#include"../Log/log.h"
#include"../sql/MyDb.h"
#include"../function.h"
#include"../pool/ClientCon.h"
#include"../pool/UpDownCon.h"
#include"srtool.h"

class AbstractTool{
public:
    AbstractTool()=default;
    virtual ~AbstractTool(){}
    virtual int doingTask()=0;
protected:
    SR_Tool sr_tool;
    std::string getSuffix(const std::string& filename); //获取文件后缀名
};






#endif
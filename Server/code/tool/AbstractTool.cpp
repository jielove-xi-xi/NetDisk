#include "AbstractTool.h"

std::string AbstractTool::getSuffix(const std::string &filename)
{
    if (filename.empty())
        return "";  // 如果文件名为空，直接返回空字符串

    size_t pos = filename.rfind('.');
    if (pos != std::string::npos && pos != filename.length() - 1) {
        return filename.substr(pos + 1);  // 找到后缀并返回
    }
    return "other";  // 如果没有后缀或者点在最后，返回空字符串
}

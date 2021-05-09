#ifndef LOGWRITER_H
#define LOGWRITER_H

#include <time.h>
#include <string.h>
#include <list>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include "Mutex/Cond.h"

#define LOGSTR_MAX_LENGTH 512

/**
 * @brief The LogWriter class
 * 写日志类 负责定时将日志信息写入文件  并管理日志文件
 */
class LogWriter {
public:
    struct LogInfoNode {
        int camera_id;
        uint64_t create_time; //创建的时间(用来判断过了多久)
        std::string log_str;

        LogInfoNode() {
            camera_id = 0;
        }
    };

    LogWriter();
    ~LogWriter();
    void writeLog(int camera_id, const std::string &str);
    void run();

private:
    void addLogNode(const LogInfoNode &node);

    char file_name_[20];
    char *tmp_buffer_;
    std::list<LogInfoNode> log_node_list_; //数据队列
    Cond *condition_;
};
#endif // LOGWRITER_H

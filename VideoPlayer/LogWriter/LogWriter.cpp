#include "LogWriter.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "types.h"

#ifdef WIN32
#include <direct.h>
#include <io.h>                      //C (Windows)    access
#define R_OK 0
#else
#include <unistd.h>                  //C (Linux)      access
#endif

#if defined(WIN32)
#include <WinSock2.h>
#include <Windows.h>
static DWORD WINAPI thread_Func(LPVOID pM)
#else
#include <sys/time.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
static void *thread_Func(void *pM)
#endif
{
    LogWriter *pointer = (LogWriter*)pM;
    pointer->run();
    return 0;
}

#define TMPBUFFERLEN (1024 * 1024 * 3)

LogWriter::LogWriter() {
    condition_ = new Cond;
    tmp_buffer_ = new char[TMPBUFFERLEN];
#if defined(WIN32)
    HANDLE handle = CreateThread(NULL, 0, thread_Func, this, 0, NULL);
#else
    pthread_t thread1;
    pthread_create(&thread1, NULL, thread_Func, this);
#endif
}

LogWriter::~LogWriter() {
    if (tmp_buffer_ != NULL) {
        delete tmp_buffer_;
        tmp_buffer_ = NULL;
    }

    if (condition_ != NULL) {
        delete condition_;
        condition_ = NULL;
    }
}

void LogWriter::addLogNode(const LogInfoNode &node) {
    condition_->Lock();
    log_node_list_.push_back(node);
    condition_->Signal();
    condition_->Unlock();
}

void LogWriter::writeLog(int camera_id, const std::string &str) {
    LogInfoNode node;
    node.camera_id = camera_id;
    node.create_time = getTimeStamp_MilliSecond();
#if defined(WIN32)
    SYSTEMTIME sys;
    GetLocalTime(&sys);
    memset(tmp_buffer_, 0x0, TMPBUFFERLEN);
    sprintf(tmp_buffer_, "[%d-%02d-%02d %02d:%02d:%02d.%03d] %s\n",
            sys.wYear, sys.wMonth, sys.wDay, sys.wHour, sys.wMinute, sys.wSecond, sys.wMilliseconds, str.c_str());
    node.log_str = tmp_buffer_;
#else
    struct timeval    tv;
    struct timezone tz;
    struct tm         *p;
    gettimeofday(&tv, &tz);
    p = localtime(&tv.tv_sec);
    memset(tmp_buffer_, 0x0, TMPBUFFERLEN);
    sprintf(tmp_buffer_, "[%d-%02d-%02d %02d:%02d:%02d.%03d] %s\n",
            1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, tv.tv_usec, str.c_str());
    node.log_str = tmp_buffer_;
#endif
    addLogNode(node);
    {
#if defined(WIN32)
        fprintf(stderr, "[%d-%02d-%02d %02d:%02d:%02d.%03d] %s\n",
                sys.wYear, sys.wMonth, sys.wDay, sys.wHour, sys.wMinute, sys.wSecond, sys.wMilliseconds, str.c_str());
#else
        fprintf(stderr, "[%d-%02d-%02d %02d:%02d:%02d.%03d] %s",
                1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, tv.tv_usec, str.c_str());
#endif
    }
}

void LogWriter::run() {
    while (1) {
        condition_->Lock();

        if (log_node_list_.empty()) {
            condition_->Wait();
        }
        bool isNeedWriteToFile = false;
        //日志文件超过10条 则写入文件
        if (log_node_list_.size() >= 10) {
            isNeedWriteToFile = true;
        } else {
            uint64_t startTime = log_node_list_.front().create_time;
            uint64_t currentTime = getTimeStamp_MilliSecond();
            //日志数据最迟10秒写入文件
            if ((currentTime - startTime) > (10000)) {
                isNeedWriteToFile = true;
            }
        }

        if (isNeedWriteToFile) {
            std::list<LogInfoNode> LogNodeList = log_node_list_;
            log_node_list_.clear();
            condition_->Unlock();

            while (!LogNodeList.empty()) {
                LogInfoNode node = LogNodeList.front();
                LogNodeList.pop_front();
#ifdef WIN32
                char logDirName[20] = {0};
                sprintf(logDirName, "log\\%d", node.camera_id);
                ///如果log目录不存在 则创建
                if (access(logDirName, R_OK) != 0) {
                    char cmd[32] = {0};
                    sprintf(cmd, "mkdir %s", logDirName);
                    system(cmd);
                }
#else
                char logDirName[20] = {0};
                sprintf(logDirName, "log/%d", node.camera_id);
                ///如果log目录不存在 则创建
                if (access(logDirName, R_OK) != 0) {
                    char cmd[32] = {0};
                    sprintf(cmd, "mkdir %s -p", logDirName);
                    system(cmd);
                }
#endif
                ///一个文件最多5M 超过5M则创建下一个文件
                int index = 0;
                char file_name_[36];
                while (1) {
                    memset(file_name_, 0x0, 36);
                    sprintf(file_name_, "log/%d/logfile_%d", node.camera_id, index++);
                    if (access(file_name_, R_OK) == 0) {
                        FILE * fp = fopen(file_name_, "r");
                        fseek(fp, 0L, SEEK_END);
                        int size = ftell(fp);
                        fclose(fp);
                        //小于5M则可以写
                        if (size < 5 * 1024 * 1024) {
                            break;
                        }
                    } else {
                        break;
                    }
                }
                FILE * fp = fopen(file_name_, "at+");
                if (fp == NULL) {
                    fprintf(stderr, "写日志失败，请确保你有足够的权限写!\n");
                } else {
                    fwrite(node.log_str.c_str(), 1, node.log_str.size(), fp);
                    fclose(fp);
                }
            }
        } else {
            condition_->Unlock();
            mSleep(5000);
            continue;
        }
    }
}
#include "types.h"
#include <time.h>
#if defined(WIN32)
#include <WinSock2.h>
#include <Windows.h>
#include <direct.h>
#include <io.h> //C (Windows)    access
#else
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>

void Sleep(long mSeconds) {
    usleep(mSeconds * 1000);
}
#endif

void mSleep(int second) {
#if defined(WIN32)
    Sleep(second);
#else
    usleep(second * 1000);
#endif
}

int64_t getTimeStamp_MilliSecond() {
    int second = 0; //µ±«∞∫¡√Î ˝
#if defined(WIN32)
    SYSTEMTIME sys;
    GetLocalTime(&sys);
    second = sys.wMilliseconds;
#else
    struct timeval    tv;
    struct timezone tz;
    struct tm         *p;
    gettimeofday(&tv, &tz);
    p = localtime(&tv.tv_sec);
    second = tv.tv_usec / 1000;
#endif
    int64_t time_stamp = ((int64_t)time(NULL)) * 1000 + second;
    return time_stamp;
}
#ifndef GLOBAL_H
#define GLOBAL_H

#ifdef _WIN32
    #include <process.h>
    #include <windows.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

extern int level;

#define TRUE 1
#define FALSE 0

#define URLMAXSIZE 100
#define IPMAXSIZE 16
#define LEN 512

#define TIMEMOD 1000

// 秒数
extern int timeCircle;
// 计时器
#ifdef __linux__
void* timePass();
#else
void* timePass();
#endif

#endif

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h> // for usleep
#endif
#include <stdio.h>
#include "global.h"
#include "IdConvert.h"

// 调试等级
int level;

// 时间
int timeCircle;

void* timePass() {
    timeCircle = 0;
    while (1) {
        #ifdef _WIN32
            Sleep(1000);
        #else
            usleep(1000000); // 1000000微秒等于1秒
        #endif
        timeCircle = (timeCircle + 1) % TIMEMOD;
    }
}

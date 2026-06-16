#include "IdConvert.h"
#include "global.h"
#include <stdlib.h>
#include <string.h>

#define ID_SIZE 1000
#define TTL 10

//初始化ID转换表
void initializeTableID(ID_Table* ID_table) {
    //分配内存
    ID_table->records = (ID_Table_Record*)malloc(sizeof(ID_Table_Record) * ID_SIZE);

    ID_table->index = 0;
    for (int i = 0; i < ID_SIZE; i++) {
        //URLMAX=100字节
        ID_table->records[i].url = (char*)malloc(sizeof(char) * URLMAXSIZE);
        ID_table->records[i].urlLength = 0;       //空标志
        ID_table->records[i].Question_id = 0;     // 原始客户端 ID（初始无意义）
        ID_table->records[i].finished = TRUE;     // TRUE = 槽位空闲
        ID_table->records[i].time = -1;           // -1 = 未设置时间戳

        memset(&(ID_table->records[i].client_addr), 0, sizeof(struct sockaddr_in));
        memset(ID_table->records[i].buf, 0, LEN);
        ID_table->records[i].length = 0;
    }
}

//超时处理
void findOutOfTime(ID_Table* ID_table, int my_socket) {
    for (int i = 0; i < ID_SIZE; i++) {
        //1.urlLength > 0
        //2.当前时间 - 记录时间 > TTL
        //3.finished == FALSE
        if (ID_table->records[i].urlLength > 0 && 
            ((timeCircle - ID_table->records[i].time + TIMEMOD) % TIMEMOD) > TTL&&
            ID_table->records[i].finished == FALSE) {

            //构造错误报文
            char errorResponse[LEN] = { 0 };
            int errorResponseLength = CreateErrorResponse(ID_table->records[i].buf,
                ID_table->records[i].length,
                errorResponse);

            int sendLength = sendto(my_socket,
                errorResponse,
                errorResponseLength,
                0,
                (struct sockaddr*)&ID_table->records[i].client_addr,
                sizeof(ID_table->records[i].client_addr));

            printf("[WARNING] Time:%d Timeout for url=%s, sent error response to client.\n",
                timeCircle, ID_table->records[i].url);

            //释放槽位
            ID_table->records[i].finished = TRUE;
        }
    }
}

//从上游 DNS 响应中恢复客户端信息
ID_Table_Record* IDFromServerToClient(ID_Table* ID_table, unsigned short transID) {
    //越界检查
    if (transID >= ID_SIZE) {
        return NULL;
    }
    if (ID_table->records[transID].finished == FALSE) {
        ID_table->records[transID].finished = TRUE;
        // 调试日志：打印 Server ID → Client ID 转换
        if (level >= 1) {
            printf("[ID Transfer] Server ID: R%d -> client ID: R%d\n",
                transID, ID_table->records[transID].Question_id);
        }
        return &(ID_table->records[transID]);
    }
    return NULL;
}

//将客户端查询存入 ID 表，分配 transID 并替换报文 ID
unsigned short IDFromClientToServer
(ID_Table* ID_table, char* buf, int length, unsigned short ID,
    struct sockaddr_in client_addr, int my_socket) {

    //初始-1表示未找到,最大尝试次数为5
    unsigned short transID = -1;
    int tryCount = 5;

    unsigned short i = ID_table->index;
    findOutOfTime(ID_table, my_socket);
    do {

        //5秒扫一次
        if (timeCircle % 5 == 0) {
            findOutOfTime(ID_table, my_socket);
            tryCount--;
        }

        if (ID_table->records[i].finished == TRUE) {
            transID = i;
            if (level >= 1) {
                printf("[ID Transfer] Client ID: Q%d -> server ID: Q%d\n", ID, transID);
            }
            //将 DNS 报文中的 ID 替换为表索引
            unsigned short packetID = htons(transID);
            memcpy(buf, &packetID, sizeof(unsigned short));
            parseDomainFromDnsPacket(buf, ID_table->records[i].url);
            ID_table->records[i].urlLength = strlen(ID_table->records[i].url);

            ID_table->records[i].Question_id = ID;            // 客户端原始 DNS ID
            ID_table->records[i].client_addr = client_addr;   // 客户端地址（用于转发响应）
            ID_table->records[i].finished = FALSE;            // 标记为"等待响应"
            ID_table->records[i].time = timeCircle;           // 记录入表时间戳
            memcpy(ID_table->records[i].buf, buf, length);    // 保存请求报文副本（超时时用）
            ID_table->records[i].length = length;             // 请求报文长度

            break;
        }

        i = (i + 1) % ID_SIZE;

    } while (tryCount > 0);
    if (tryCount >= 0) {
        ID_table->index = (i + 1) % ID_SIZE;
    }
    else if (tryCount == 0) {
        transID = -1;
    }
    return transID;
}

//释放表内存
void free_ID(ID_Table* ID_table) {
    for (int i = 0; i < ID_SIZE; i++) {
        free(ID_table->records[i].url);
    }
    free(ID_table->records);
    free(ID_table);
}
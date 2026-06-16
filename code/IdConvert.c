#include"IdConvert.h"
#include"global.h"
#include <stdlib.h>
#include <string.h>

#define ID_SIZE 1000
#define TTL 10


// 初始化
void initializeTableID(ID_Table* ID_table)
{
    ID_table->records = (ID_Table_Record*)malloc(sizeof(ID_Table_Record) * ID_SIZE);
    ID_table->index = 0;
    for (int i = 0; i < ID_SIZE; i++)
    {
        ID_table->records[i].url = (char*)malloc(sizeof(char) * URLMAXSIZE);
        ID_table->records[i].urlLength = 0;
        ID_table->records[i].Question_id = 0;
        ID_table->records[i].finished = TRUE;
        ID_table->records[i].time = -1;
        memset(&(ID_table->records[i].client_addr), 0, sizeof(struct sockaddr_in));
        memset(ID_table->records[i].buf, 0, LEN); // 初始化请求数据缓冲区
        ID_table->records[i].length = 0;          // 初始化请求数据长度
    }
}

// 超时处理
void findOutOfTime(ID_Table* ID_table, int my_socket) {
    for (int i = 0; i < ID_SIZE; i++) {
        if (ID_table->records[i].urlLength > 0 && 
            ((timeCircle - ID_table->records[i].time + TIMEMOD) % TIMEMOD) > TTL && 
            ID_table->records[i].finished == FALSE) {

            // 生成错误报文
            char errorResponse[LEN] = {0};
            int errorResponseLength = CreateErrorResponse(ID_table->records[i].buf, ID_table->records[i].length, errorResponse);

            // 发送错误报文给客户端
            int sendLength = sendto(my_socket, errorResponse, errorResponseLength, 0, 
                                    (struct sockaddr*)&ID_table->records[i].client_addr, sizeof(ID_table->records[i].client_addr));
            printf("[WARNING] Time:%d Timeout for url=%s, sent error response to client.\n",timeCircle, ID_table->records[i].url);
            // 标记为已完成
            ID_table->records[i].finished = TRUE;
        }
    }
}


// client<-relay<-server 根据transID(索引)寻找ID表项并转换为原ID
ID_Table_Record* IDFromServerToClient(ID_Table* ID_table, unsigned short transID)
{
    // 无效的DNS包，直接丢弃
    if(transID >= ID_SIZE)
        return NULL;

    // client->server时，transID是ID表的索引值，那么我只需要根据该索引就能找到原ID
    if(ID_table->records[transID].finished == FALSE){
        // 标记已完成
        ID_table->records[transID].finished = TRUE;


        // 打印 server 和 client ID
        if(level >= 1) {
            printf("[ID Transfer] Server ID: R%d -> client ID: R%d\n", transID, ID_table->records[transID].Question_id);
        }
        

        return &(ID_table->records[transID]);
    }
        
    return NULL;
}

// 向ID表中保存新的DNS查询记录
unsigned short IDFromClientToServer(ID_Table* ID_table, char* buf, int length, unsigned short ID, struct sockaddr_in client_addr, int my_socket)
{

    // 声明并初始化一个无符号短整型变量，用于存储新记录的ID
    unsigned short transID = -1;

    // 尝试次数
    int tryCount = 5;

    // 初始化一个整型变量，将其设为ID表的上次最后索引值
    unsigned short i = ID_table->index;

    // 先检查并处理超时请求
    findOutOfTime(ID_table, my_socket);
    
    // 使用 do-while 循环来遍历ID表中的记录，直到超时有空位置
    do {

        // 每 5秒 判断是否有超时
        if(timeCircle % 5 == 0) {
            findOutOfTime(ID_table, my_socket);
            tryCount--;
        }
        

        // 如果当前记录已完成，则说明此处有空间可存储新的记录
        if (ID_table->records[i].finished == TRUE) {
            // 当前记录可用，即使用当前记录，transID设为i
            transID = i;

            // 打印 client 和 server ID
            if(level >= 1) {
                printf("[ID Transfer] Client ID: Q%d -> server ID: Q%d\n", ID, transID);
            }
            
            unsigned short packetID = htons(transID);
            // 将新记录的ID复制到缓冲区中，以替换原始查询中的ID
            memcpy(buf, &packetID, sizeof(unsigned short));

            // 把packet中的url解析到记录中，并且更新url长度
            parseDomainFromDnsPacket(buf, ID_table->records[i].url);
            ID_table->records[i].urlLength = strlen(ID_table->records[i].url);


            // 将新记录的查询ID设置为传入的参数 ID
            ID_table->records[i].Question_id = ID;
            // 将新记录的客户端地址设置为传入的参数 client_addr
            ID_table->records[i].client_addr = client_addr;
            // 将新记录标记为未完成
            ID_table->records[i].finished = FALSE;
            // 将新记录的时间戳更新为当前时间
            ID_table->records[i].time = timeCircle;    // 0-999
            memcpy(ID_table->records[i].buf, buf, length);   // 保存请求数据
            ID_table->records[i].length = length;            // 保存请求数据长度
            
            break;
        }

        // 如果在之前已经找到可用记录，那么已经break了
        // 如果运行到这里说明没有还没找到可用记录，inc(i)继续遍历
        i = (i + 1) % ID_SIZE;
        
    } while (tryCount > 0);    // 耗尽结束

    // 更新ID表的索引值，使其指向下一个位置
    if (tryCount >= 0) {
        ID_table->index = (i + 1) % ID_SIZE;
    }
    else if (tryCount == 0) {
        transID = -1;
    }
    
    
    // 返回新记录的ID
    return transID;
}

// 释放
void free_ID(ID_Table* ID_table) {
    for (int i = 0; i < ID_SIZE; i++) {
        free(ID_table->records[i].url);
    }
    free(ID_table->records);
    free(ID_table);
}
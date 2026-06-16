#ifndef IDCONVERT_H
#define IDCONVERT_H

#include"DnsPacket.h"
#include"global.h"
#include"stdio.h"

#ifdef _WIN32
	#include<winsock2.h>
    typedef BOOL bool;
	#pragma comment(lib,"wsock32.lib")
#endif

#ifdef __linux__
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <sys/types.h>
    typedef int bool;
    #define TRUE 1
    #define FALSE 0
#endif

// ID表项
typedef struct {
    char* url;                       // domain
    int urlLength;                   // domain length
    unsigned short Question_id;      // client->server id
    struct sockaddr_in client_addr;         // client address
    int time;                        // time enter
    bool finished;                   // False if not finished(can not use)
    char buf[LEN];                   // 存储请求数据
    int length;                      // 请求数据长度
} ID_Table_Record;


typedef struct
{
    ID_Table_Record* records;   //record数组
    int index;                  //最后使用下标
} ID_Table;

void initializeTableID(ID_Table* ID_table);
ID_Table_Record* IDFromServerToClient(ID_Table* ID_table, unsigned short ID);
unsigned short IDFromClientToServer(ID_Table* ID_table, char* buf, int length, unsigned short ID, struct sockaddr_in client_addr, int my_socket);
void findOutOfTime(ID_Table* ID_table, int my_socket);

#endif
#ifndef MAIN_H
#define MAIN_H

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<time.h>

#ifdef _WIN32
    #include <process.h>
    #include <winsock2.h>
    #include <Ws2tcpip.h>
    #pragma comment(lib,"wsock32.lib")
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <arpa/inet.h>
#endif

#include "DnsPacket.h"
#include "header.h"
#include "IdConvert.h"
#include "LocalHash.h"
#include "LruCache.h"
#include "global.h"

// 函数声明
void set_commandInfo(int argc, char* argv[], char* server_ip, char* file_name);
void initialize_socket(char* server_ip);
void initialize_all(int argc, char* argv[], char* server_ip, char* file_name, ID_Table** ID_table, Cache** cache, FILE** dnsFile, HashTable** hashTable);
void handle_server_packet(char *buf, int length, ID_Table *ID_table, Cache *cache, FILE *dnsFile, HashTable *hashTable);
void handle_client_packet(char *buf, int length, ID_Table *ID_table, Cache *cache, HashTable *hashTable);

// 套接字变量
int my_socket;                  // 套接字
struct sockaddr_in client_addr; // 客户端套接字地址
struct sockaddr_in server_addr; // 服务器套接字地址
struct sockaddr_in tmp_sockaddr;// 接收暂存地址
int sockaddr_in_size = sizeof(struct sockaddr_in);

#endif

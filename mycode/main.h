#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
    #include <process.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
#pragma comment(lib, "wscok32.lib")
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

void set_commandInfo(int argc, char* argv[], char* server_ip, char* file_name);

void initialize_socket(char* server_ip);

void initialize_all(int argc, char* argv[], char* server_ip, char* file_name,
                    ID_Table** ID_table, Cache** cache, FILE** dnsFile, HashTable** hashTable);

void handle_server_packet(char *buf, int length, ID_Table *ID_table, Cache *cache,
                          FILE *dnsFile, HashTable *hashTable);

void handle_client_packet(char *buf, int length, ID_Table *ID_table, Cache *cache,
                          HashTable *hashTable);

int my_socket;

struct sockaddr_in client_addr;

struct sockaddr_in server_addr;

struct sockaddr_in tmp_sockaddr;

int sockaddr_in_size = sizeof(struct sockaddr_in);

#endif
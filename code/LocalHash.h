#ifndef LOCALHASH_H
#define LOCALHASH_H

#include <stdio.h>
#include "global.h"

// 哈希表结构体
typedef struct HashTable HashTable;

// 哈希表节点结构体
typedef struct HashNode {
    char** ips; // 存储多个IP
    int ipCount;
    char* url;
    struct HashNode* next;
} HashNode;

// 初始化哈希表
HashTable* initHashTable();

// 向哈希表中插入节点
void insertHashTable(HashTable* hashTable, char* ip, char* url);

// 从文件中读取IP-URL转换表，并构建哈希表
void buildHashTableFromFile(FILE* relayTable, HashTable* hashTable);


// 向文件中添加新的IP-URL转换记录
void addEntryToRelayTable(FILE *relayTable, const char *ip, const char *url);

int findIPlocallyMultiple(char* domain, HashTable* hashTable, char* ips[], int* ipCount);

#endif /* HASH_H */


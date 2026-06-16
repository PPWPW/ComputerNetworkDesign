#ifndef LOCALHASH_H
#define LOCALHASH_H

#include <stdio.h>
#include "global.h"

typedef struct HashTable HashTable;
typedef struct HashNode {
    char** ips;   //IP 地址字符串数组
    int ipCount;  //IP地址数量
    char* url;    //域名
    struct HashNode* next;   //链表指针
}HashNode;

HashTable* initHashTable();

void insertHashTable(HashTable* hashTable, char* ip, char* url);

void buildHashTableFromFile(FILE* relayTable, HashTable* hashTable);

void addEntryToRelayTable(FILE *relayTable, const char *ip, const char *url);

int findIPlocallyMultiple(char* domain, HashTable* hashTable, char* ips[], int* ipCount);

#endif
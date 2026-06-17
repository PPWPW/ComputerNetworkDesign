#ifndef LRUCACHE_H
#define LRUCACHE_H

#include "global.h"

//双向链表
typedef struct CacheNode {
    char* url;
    char** ips;
    int ipCount;
    struct CacheNode* prev;
    struct CacheNode* next;
}CacheNode;

typedef struct {
    int capacity;       //剩余容量
    CacheNode* head;
    CacheNode* tail;
    CacheNode** hashmap;
}Cache;

unsigned int cacheHash(char* str);

CacheNode* createCacheNode(char* url, char** ips, int ipCount);

Cache* initCache();

int findIpsAndRefresh(Cache* obj, char* url, char* ips[]);

void addCache(Cache* obj, char* url, char** ips, int ipCount);

void printCache(Cache* obj);

void freeCache(Cache* obj);

#endif
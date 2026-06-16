#ifndef LRUCACHE_H
#define LRUCACHE_H

#include "global.h"

// 定义LRU Cache节点结构
typedef struct CacheNode{
    char* url;
    char** ips; // 修改为存储多个IP
    int ipCount;
    struct CacheNode* prev;
    struct CacheNode* next;
} CacheNode;

// 定义LRU Cache缓存结构
typedef struct {
    int capacity;
    CacheNode* head;
    CacheNode* tail;
    CacheNode** hashmap;
} Cache;

// 哈希函数
unsigned int cacheHash(char* str);

// 创建一个新节点
CacheNode* createCacheNode(char* url, char** ips, int ipCount);

// 初始化LRU缓存
Cache* initCache();

// 在Cache中查找URL对应的IP并更新LRU顺序
int findIpsAndRefresh(Cache* obj, char* url, char* ips[]);

// 向Cache中添加记录，如果记录已存在Cache中，则更新，并将其放在Cache顶部
void addCache(Cache* obj, char* url, char** ips, int ipCount);

// 格式化打印缓存中的键值对
void printCache(Cache* obj);

// 释放LRU缓存
void freeCache(Cache* obj);


#endif
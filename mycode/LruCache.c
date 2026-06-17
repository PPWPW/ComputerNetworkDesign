#include <stdio.h>
#include <stdlib.h>
#include "LruCache.h"

#define HASH_TABLE_SIZE 10000     //哈希表大小
#define CACHE_CAPACITY 30         //LRU缓存节点数

//创建新缓存节点
CacheNode* createCacheNode(char* url, char** ips, int ipCount) {

    CacheNode* newNode = (CacheNode*)malloc(sizeof(CacheNode));
    newNode->url = strdup(url);
    newNode->ips = (char**)malloc(sizeof(char*) * ipCount);
    for (int i = 0; i < ipCount; i++) {
        newNode->ips[i] = strdup(ips[i]);
    }
    newNode->ipCount = ipCount;
    newNode->prev = NULL;
    newNode->next = NULL;

    return newNode;
}

//初始化LRU缓存
Cache* initCache() {

    Cache* cache = (Cache*)malloc(sizeof(cache));
    cache->capacity = CACHE_CAPACITY;
    cache->head = createCacheNode("", NULL, 0);
    cache->tail = createCacheNode("", NULL, 0);
    cache->head->next = cache->tail;
    cache->tail->prev = cache->head;
    cache->hashmap = (CacheNode**)malloc(sizeof(CacheNode*) * HASH_TABLE_SIZE);
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        cache->hashmap[i] = NULL;
    }
    return cache;
}

//在缓存中查找域名并刷新其LRU位置
int findIpsAndRefresh(Cache* obj, char* url, char* ips[]) {

    CacheNode* node = obj->hashmap[cacheHash(url) % HASH_TABLE_SIZE];

    //线性探测
    while (node != NULL) {
        if (strcmp(node->url, url) == 0) {

            //删除
            node->prev->next = node->next;
            node->next->prev = node->prev;
            //插入头部
            node->next = obj->head->next;
            obj->head->next->prev = node;
            node->prev = obj->head;
            obj->head->next = node;

            int ipCount = node->ipCount;
            for (int i = 0; i < ipCount; i++) {
                ips[i] = strdup(node->ips[i]);
            }

            //缓存命中信息
            if (level >= 2) {
                printf("[Cache Found] Time=%d url=%s IP count=%d. First IP=%s\n",
                       timeCircle, url, ipCount, ips[0]);
            }
            return ipCount;
        }
        node = node->next;
    }
    return 0;  //未命中
}

//向缓存中更新记录
void addCache(Cache* obj, char* url, char** ips, int ipCount) {

    //url=NULL,非法输入返回
    if (url == NULL) {
        return;
    }

    //url已存在，则更新
    CacheNode* node = obj->hashmap[cacheHash(url) % HASH_TABLE_SIZE];
    while (node != NULL) {
        if (strcmp(node->url, url) == 0) {

            //先释放旧IP列表，再存入新的
            for (int i = 0; i < node->ipCount; i++) {
                free(node->ips[i]);
            }
            free(node->ips);

            node->ips = (char**)malloc(sizeof(char*) * ipCount);
            for (int i = 0; i < ipCount; i++) {
                node->ips[i] = strdup(ips[i]);
            }
            node->ipCount = ipCount;

            node->prev->next = node->next;
            node->next->prev = node->prev;
            node->next = obj->head->next;
            obj->head->next->prev = node;
            node->prev = obj->head;
            obj->head->next = node;

            if (level >= 2) {
                printf("[Cache Update] Time=%d url=%s\n", timeCircle, node->url);
            }
            return;
        }
        node = node->next;
    }

    //url不存在，则插入
    if (obj->capacity == 0) {
        CacheNode* tailPrev = obj->tail->prev;
        obj->hashmap[cacheHash(tailPrev->url) % HASH_TABLE_SIZE] = NULL;
        tailPrev->prev->next = obj->tail;
        obj->tail->prev = tailPrev->prev;

        if (level >= 2) {
            printf("[Cache Delete] Time=%d url=%s\n", timeCircle, tailPrev->url);
        }

        free(tailPrev->url);
        for (int i = 0; i < tailPrev->ipCount; i++) {
            free(tailPrev->ips[i]);
        }
        free(tailPrev->ips);
        free(tailPrev);
    }
    else {
        obj->capacity--;
    }
    CacheNode* newNode = createCacheNode(url, ips, ipCount);

    newNode->next = obj->head->next;
    obj->head->next->prev = newNode;
    newNode->prev = obj->head;
    obj->head->next = newNode;

    if (level >= 2) {
        printf("[Cache Insert] Time=%d url=%s\n", timeCircle, newNode->url);
    }
    obj->hashmap[cacheHash(url) % HASH_TABLE_SIZE] = newNode;

    if (level >= 2) {
        printCache(obj);
    }

}

//打印缓存
void printCache(Cache* obj) {
    CacheNode* current = obj->head->next;
    printf("------[Cache content]:\n");
    while (current != obj->tail) {
        printf("URL: %s, IPs: ", current->url);
        for (int i = 0; i < current->ipCount; i++) {
            printf("%s", current->ips[i]);
        }
        printf("\n");
        current = current->next;
    }
    printf("\n");
}

//释放LRU内存
void freeCache(Cache* obj) {
    CacheNode* current = obj->head;
    while (current != NULL) {
        CacheNode* temp = current;
        current = current->next;
        free(temp->url);
        for (int i = 0; i < temp->ipCount; i++) {
            free(temp->ips[i]);
        }
        free(temp->ips);
        free(temp);
    }
    free(obj->hashmap);
    free(obj);
}

//hash = ((hash << 5) + hash) + c
//初始hash=5381, DJB2标准值
unsigned int cacheHash(char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}
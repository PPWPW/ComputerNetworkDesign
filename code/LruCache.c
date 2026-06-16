#include <stdio.h>
#include <stdlib.h>
#include "LruCache.h"


#define HASH_TABLE_SIZE 10000
#define CACHE_CAPACITY 30

// 创建一个新节点
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


// 初始化LRU缓存
Cache* initCache() {
    Cache* cache = (Cache*)malloc(sizeof(Cache));
    cache->capacity = CACHE_CAPACITY;
    cache->head = createCacheNode("", NULL, 0); // 哨兵节点
    cache->tail = createCacheNode("", NULL, 0); // 哨兵节点
    cache->head->next = cache->tail;
    cache->tail->prev = cache->head;

    // 初始化哈希表
    cache->hashmap = (CacheNode**)malloc(sizeof(CacheNode*) * HASH_TABLE_SIZE); // 假设key范围在[0, 9999]
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        cache->hashmap[i] = NULL;
    }

    return cache;
}

// 获取url对应的ip列表并刷新LRU顺序
int findIpsAndRefresh(Cache* obj, char* url, char* ips[]) {

    CacheNode* node = obj->hashmap[cacheHash(url) % HASH_TABLE_SIZE];
    while (node != NULL) {
        if (strcmp(node->url, url) == 0) {
            // 将节点移动到链表头部
            node->prev->next = node->next;
            node->next->prev = node->prev;
            node->next = obj->head->next;
            obj->head->next->prev = node;
            node->prev = obj->head;
            obj->head->next = node;

            
            int ipCount = node->ipCount;

            for (int i = 0; i < ipCount; i++) {
                ips[i] = strdup(node->ips[i]);
            }

            if (level >= 2)
            {
                printf("[Cache Found] Time=%d url=%s IP count=%d. First IP=%s\n", timeCircle, url, ipCount, ips[0]);
            }
            return ipCount;
        }
        node = node->next;
    }

    return 0;
}


// 插入或更新缓存中的键值对
void addCache(Cache* obj, char* url, char** ips, int ipCount) {
    if (url == NULL) {
        return;
    }

    CacheNode* node = obj->hashmap[cacheHash(url) % HASH_TABLE_SIZE];
    while (node != NULL) {
        if (strcmp(node->url, url) == 0) {
            // 更新节点的值
            for (int i = 0; i < node->ipCount; i++) {
                free(node->ips[i]);
            }
            free(node->ips);

            node->ips = (char**)malloc(sizeof(char*) * ipCount);
            for (int i = 0; i < ipCount; i++) {
                node->ips[i] = strdup(ips[i]);
            }
            node->ipCount = ipCount;

            // 将节点移动到链表头部
            node->prev->next = node->next;
            node->next->prev = node->prev;
            node->next = obj->head->next;
            obj->head->next->prev = node;
            node->prev = obj->head;
            obj->head->next = node;

            if (level >= 2)
            {
                printf("[Cache Update] Time=%d url=%s\n", timeCircle, node->url);
            }
            return;
        }
        node = node->next;
    }

    if (obj->capacity == 0) {
        // 删除最久未使用的节点
        CacheNode* tailPrev = obj->tail->prev;
        obj->hashmap[cacheHash(tailPrev->url) % HASH_TABLE_SIZE] = NULL;
        tailPrev->prev->next = obj->tail;
        obj->tail->prev = tailPrev->prev;

        if (level >= 2)
        {
            printf("[Cache Delete] Time=%d url=%s\n", timeCircle, tailPrev->url);
        }

        free(tailPrev->url);
        for (int i = 0; i < tailPrev->ipCount; i++) {
            free(tailPrev->ips[i]);
        }
        free(tailPrev->ips);
        free(tailPrev);
    } else {
        obj->capacity--;
    }

    // 创建新节点
    CacheNode* newNode = createCacheNode(url, ips, ipCount);

    // 插入新节点到链表头部
    newNode->next = obj->head->next;
    obj->head->next->prev = newNode;
    newNode->prev = obj->head;
    obj->head->next = newNode;

    if (level >= 2)
    {
        printf("[Cache Insert] Time=%d url=%s\n", timeCircle, newNode->url);
    }


    // 更新哈希表
    obj->hashmap[cacheHash(url) % HASH_TABLE_SIZE] = newNode;

    if (level >= 2)
    {
        printCache(obj);
    }
    
}

// 格式化打印缓存中的键值对
void printCache(Cache* obj) {
    CacheNode* current = obj->head->next;
    printf("------[Cache content]:\n");
    while (current != obj->tail) {
        printf("URL: %s, IPs: ", current->url);
        for (int i = 0; i < current->ipCount; i++) {
            printf("%s ", current->ips[i]);
        }
        printf("\n");
        current = current->next;
    }
    printf("\n");
}

// 释放LRU缓存
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

// 哈希函数
unsigned int cacheHash(char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

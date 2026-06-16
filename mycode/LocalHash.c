#include "LocalHash.h"
#include <stdlib.h>
#include <string.h>

#define TABLE_SIZE 1000

struct HashTable {
    HashNode* table[TABLE_SIZE];

};

/**
 * 字符串滚动哈希：hashValue = hashValue * 32 + c
 *最后对 TABLE_SIZE 取模，将哈希值映射到 [0, TABLE_SIZE-1]
 */
unsigned int hash(char* str) {
    unsigned int hashValue = 0;
    while (*str) {
        hashValue = (hashValue << 5) + *str++;
    }
    return hashValue % TABLE_SIZE;
}

//创建哈希表节点
HashNode* createHashNode(char* ip, char* url) {
    HashNode* newNode = (HashNode*)malloc(sizeof(HashNode));
    if (newNode) {
        newNode->ips = (char**)malloc(sizeof(char*));
        newNode->ips[0] = strdup(ip);  // 拷贝 IP 字符串
        newNode->ipCount = 1;          // 初始 IP 数量 = 1
        newNode->url = strdup(url);    // 拷贝 URL 字符串
        newNode->next = NULL;          // 下一个节点初始为 NULL
    }
    return newNode;
}

//初始化哈希表
HashNode* initHashTable() {
    HashTable* hashTable = (HashTable*)malloc(sizeof(HashTable));
    if (hashTable) {
        // 将所有槽位置 NULL
        for (int i = 0; i < TABLE_SIZE; i++) {
            hashTable->table[i] = NULL;
        }
    }
    return hashTable;
}

//向哈希表中插入映射
void insertHashTable(HashTable* hashTable, char* ip, char* url) {

    //输入验证
    if (ip == NULL || strlen(ip) <= 2 || url == NULL) {
        return;
    }

    unsigned int index = hash(url);
    HashNode* currentNode = hashTable->table[index];

    while (currentNode) {

        //URL已存在，追加IP
        if (strcmp(currentNode->url, url) == 0) {
            currentNode->ips = (char**)realloc(currentNode->ips,
                sizeof(char*) * (currentNode->ipCount + 1));
            currentNode->ips[currentNode->ipCount] = strdup(ip);
            currentNode->ipCount++;

            if (level >= 2) {
                printf("[Hash Update] Time=%d Url=%s IP=%s\n", timeCircle, url, ip);
            }
            return;
        }
        currentNode = currentNode->next;
    }

    //URL不存在，创建新节点
    HashNode* newNode = createHashNode(ip, url);

    if (level >= 2) {
        printf("[Hash Insert] Time=%d url=%s IP=%s\n", timeCircle, newNode->url, newNode->ips[0]);
    }

    if (hashTable->table[index] == NULL) {
        hashTable->table[index] = newNode;
    }
    else {
        newNode->next = hashTable->table[index];
        hashTable->table[index] = newNode;
    }

}

//在哈希表中查找域名对应的所有 IP
int findIPlocallyMultiple(char* domain, HashTable* hashTable, char* ips[], int* ipCount) {

    if (hashTable == NULL) {
        return 0;
    }

    unsigned int index = hash(domain);
    HashNode* currentNode = hashTable->table[index];
    while (currentNode) {
        if (strcmp(currentNode->url, domain) == 0) {
            *ipCount = currentNode->ipCount;
            for (int i = 0; i < *ipCount; i++) {
                ips[i] = strdup(currentNode->ips[i]);
            }
            return *ipCount;
        }
        currentNode=currentNode->next;
    }

    //未找到
    *ipCount = 0;
    return 0;

}

//从文件构建哈希表
void buildHasnTableFromFile(FILE* relayTable, HashTable* hashTable) {

    char line[256];   //行缓冲区
    char ip[20];      //IP字符缓冲
    char url[100];    //URL缓冲
    while (fgets(line, sizeof(line), relayTable) != NULL) {
        sscanf(line, "%s %s", ip, url);
        insertHashTable(hashTable, ip, url);
    }
}

//向 relay 文件追加新的映射
void addEntryToRelayTable(FILE* relayTable, const char* ip, const char* url) {

    if (relayTable == NULL) {
        return;
    }

    if (ip == NULL || strlen(ip) <= 2 || url == NULL) {
        return ;
    }
    //指针移动到末尾
    fseek(relayTable, 0, SEEK_END);
    fprintf(relayTable, "%s %s\n", ip, url);
    fflush(relayTable);
    if (level >= 2) {
        printf("[Database Insert] Time=%d url=%s IP=%s\n", timeCircle, url, ip);
    }
}
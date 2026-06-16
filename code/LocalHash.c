#include "LocalHash.h"
#include <stdlib.h>
#include <string.h>

#define TABLE_SIZE 1000 // 哈希表大小

// 哈希表结构体
struct HashTable {
    HashNode* table[TABLE_SIZE];
};

// 哈希函数：计算字符串的哈希值
unsigned int hash(char* str) {
    unsigned int hashValue = 0;
    while (*str) {
        hashValue = (hashValue << 5) + *str++;
    }
    return hashValue % TABLE_SIZE;
}

// 创建哈希节点
HashNode* createHashNode(char* ip, char* url) {
    HashNode* newNode = (HashNode*)malloc(sizeof(HashNode));
    if (newNode) {
        newNode->ips = (char**)malloc(sizeof(char*));
        newNode->ips[0] = strdup(ip);
        newNode->ipCount = 1;
        newNode->url = strdup(url);
        newNode->next = NULL;
    }
    return newNode;
}

// 初始化哈希表
HashTable* initHashTable() {
    HashTable* hashTable = (HashTable*)malloc(sizeof(HashTable));
    if (hashTable) {
        for (int i = 0; i < TABLE_SIZE; i++) {
            hashTable->table[i] = NULL;
        }
    }
    return hashTable;
}

// 向哈希表中插入节点
void insertHashTable(HashTable* hashTable, char* ip, char* url) {
    if (ip == NULL || strlen(ip) <= 2 || url == NULL) {
        return;
    }
    unsigned int index = hash(url);
    HashNode* currentNode = hashTable->table[index];
    while (currentNode) {
        if (strcmp(currentNode->url, url) == 0) {
            // 如果URL已存在，添加新的IP到现有节点中
            currentNode->ips = (char**)realloc(currentNode->ips, sizeof(char*) * (currentNode->ipCount + 1));
            currentNode->ips[currentNode->ipCount] = strdup(ip);
            currentNode->ipCount++;
            if (level >= 2)
            {
                printf("[Hash Update] Time=%d Url=%s IP=%s\n", timeCircle, url, ip);
            }
            
            return;
        }
        currentNode = currentNode->next;
    }
    // URL不存在，创建新的节点并插入哈希表
    HashNode* newNode = createHashNode(ip, url);
    if (level >= 2)
    {
        printf("[Hash Insert] Time=%d url=%s IP=%s\n", timeCircle, newNode->url, newNode->ips[0]);
    }
    

    if (hashTable->table[index] == NULL) {
        hashTable->table[index] = newNode;
    } else {
        // 在链表头部插入新节点,确保首个是最新的
        newNode->next = hashTable->table[index];
        hashTable->table[index] = newNode;
    }
}

// 在哈希表中查找节点并返回IP数量
int findIPlocallyMultiple(char* domain, HashTable* hashTable, char* ips[], int* ipCount) {
    if (hashTable == NULL) {
        return 0; // 哈希表为空，返回0个IP
    }
    unsigned int index = hash(domain);
    HashNode* currentNode = hashTable->table[index];
    while (currentNode) {
        if (strcmp(currentNode->url, domain) == 0) {
            *ipCount = currentNode->ipCount;
            for (int i = 0; i < *ipCount; i++) {
                ips[i] = strdup(currentNode->ips[i]);
            }
            return *ipCount; // 返回找到的IP数量
        }
        currentNode = currentNode->next;
    }
    *ipCount = 0;
    return 0; // 没有找到对应的URL，返回0个IP
}


// 从文件中读取IP-URL转换表，并构建哈希表
void buildHashTableFromFile(FILE* relayTable, HashTable* hashTable) {
    char line[256];
    char ip[20];
    char url[100];
    while (fgets(line, sizeof(line), relayTable) != NULL) {
        sscanf(line, "%s %s", ip, url);
        insertHashTable(hashTable, ip, url);
    }
}


// 向文件中添加新的IP-URL转换记录
void addEntryToRelayTable(FILE *relayTable, const char *ip, const char *url) {
    if (relayTable == NULL) {
        return;
    }

    if (ip == NULL || strlen(ip) <= 2 || url == NULL) {
        return ;
    }

    // 移动文件指针到文件末尾
    fseek(relayTable, 0, SEEK_END);

    // 写入新的IP和URL
    fprintf(relayTable, "%s %s\n", ip, url);

    // 刷新文件缓冲区以确保数据写入文件
    fflush(relayTable);

    if (level >= 2)
    {
        printf("[Database Insert] Time=%d url=%s IP=%s\n", timeCircle, url, ip);
    }
}


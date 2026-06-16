#include "main.h"


int main(int argc, char* argv[]) {
    // 默认服务器IP
    char server_ip[IPMAXSIZE] = "10.3.9.6";
    // 文件名
    char file_name[200] = "dnsrelay.txt";
    
    // 初始化ID表、缓存、本地文件和哈希表
    ID_Table* ID_table;
    Cache* cache;
    FILE* dnsFile;
    HashTable* hashTable;
    initialize_all(argc, argv, server_ip, file_name, &ID_table, &cache, &dnsFile, &hashTable);

    char buf[LEN] = { 0 };

    while (1) {
        // 检查并处理超时请求
        findOutOfTime(ID_table, my_socket);

        // 清空缓冲区
        memset(buf, '\0', LEN);

        // 接收数据包，使用暂存地址保存
        int length = recvfrom(my_socket, buf, sizeof(buf), 0, (struct sockaddr*)&tmp_sockaddr, &sockaddr_in_size);
        HEADER* p = (struct HEADER*)buf;

        // 根据包的类型处理
        if (p->qr == 1) {
            handle_server_packet(buf, length, ID_table, cache, dnsFile, hashTable);
        } else if (p->qr == 0) {
            client_addr = tmp_sockaddr;
            handle_client_packet(buf, length, ID_table, cache, hashTable);
        }
    }

    close(my_socket);
}

// 初始化所有组件的函数
void initialize_all(int argc, char* argv[], char* server_ip, char* file_name, ID_Table** ID_table, Cache** cache, FILE** dnsFile, HashTable** hashTable) {
    // 设置命令行参数
    set_commandInfo(argc, argv, server_ip, file_name);

    // 初始化ID表
    *ID_table = (ID_Table*)malloc(sizeof(ID_Table));
    initializeTableID(*ID_table);

    // 初始化缓存
    *cache = initCache();

    // 初始化本地文件和哈希表
    *dnsFile = fopen(file_name, "r+");
    if (*dnsFile == NULL) {
        printf("[Error] %s not exist!\n", file_name);
    } else {
        *hashTable = initHashTable();
        buildHashTableFromFile(*dnsFile, *hashTable);
    }

    // 初始化套接字
#ifdef _WIN32
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);
#endif
    initialize_socket(server_ip);

    // 启动定时器线程
#ifdef _WIN32
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, timePass, NULL, 0, NULL);
#else
    pthread_t hThread;
    pthread_create(&hThread, NULL, timePass, NULL);
#endif
}

// 处理来自服务器的数据包
void handle_server_packet(char *buf, int length, ID_Table *ID_table, Cache *cache, FILE *dnsFile, HashTable *hashTable) {
    unsigned short id = ntohs(((HEADER*)buf)->id);  // 从包中提取ID
    char url[URLMAXSIZE];  // 用于存储URL的缓冲区
    char* curIPs[100];  // 用于存储IP地址的缓冲区
    int ipCount = 0;  // IP地址数量

    // 从DNS包中解析域名
    parseDomainFromDnsPacket(buf, url);
    if (level >= 1) {
        printf("\n[Receive] Time=%d Received from Server. TypeA=%d ID=%d Url=%s\n", timeCircle, isFirstQueryTypeA(buf), id, url);
    }

    // 从ID表中找到对应的记录
    ID_Table_Record* req = IDFromServerToClient(ID_table, id);
    if (req != NULL) {
        id = req->Question_id;  // 转换为客户端的ID
        unsigned short packetID = htons(id);
        memcpy(buf, &packetID, sizeof(unsigned short));  // 将转换后的ID写回包中

        // 将数据包发送给客户端
        int sendLength = sendto(my_socket, buf, length, 0, (struct sockaddr*)&req->client_addr, sizeof(req->client_addr));
        if (level >= 1) {
            printf("\n[Send] Time=%d Send to Client. ID=%d Url=%s\n", timeCircle, id, url);
        }

        // 如果是A记录查询，提取IP并更新缓存和本地文件
        if (isFirstQueryTypeA(buf)) {
            extractIpsFromDnsPacket(buf, length, curIPs, &ipCount);
            if (curIPs[0] != NULL) {
                addCache(cache, url, curIPs, ipCount);
                if (dnsFile != NULL) {
                    for (int i = 0; i < ipCount; i++) {
                        addEntryToRelayTable(dnsFile, curIPs[i], url);
                        insertHashTable(hashTable, curIPs[i], url);
                    }
                }
            }
        }
    } else {
        printf("[Warning] Time:%d Packet ID:%d from Server has Invalid ID (May be Timeout), Discard\n");
    }
}

// 处理来自客户端的数据包
void handle_client_packet(char *buf, int length, ID_Table *ID_table, Cache *cache, HashTable *hashTable) {
    unsigned short id = ntohs(((HEADER*)buf)->id);  // 从包中提取ID
    char url[URLMAXSIZE];  // 用于存储URL的缓冲区
    char* curIPs[100];  // 用于存储IP地址的缓冲区
    int ipCount = 0;  // IP地址数量

    // 从DNS包中解析域名
    parseDomainFromDnsPacket(buf, url);
    if (level >= 1) {
        printf("\n[Receive] Time=%d Received from Client. TypeA=%d ID=%d Url=%s\n", timeCircle, isFirstQueryTypeA(buf), id, url);
    }

    // 处理非空请求
    if (url != NULL && length >= 12) {
        int flag = 0;  // 标记是否找到URL
        int forbidden = 0;  // 标记是否禁止URL

        char* localIps[100];  // 用于存储本地IP地址的缓冲区
        int localIpCount = 0;  // 本地IP地址数量
        memset(localIps, 0, sizeof(localIps));
        findIPlocallyMultiple(url, hashTable, localIps, &localIpCount);

        // 检查是否禁止URL
        if (localIpCount > 0) {
            for (int i = 0; i < localIpCount; i++) {
                if (strcmp(localIps[i], "0.0.0.0") == 0) {
                    forbidden = 1;
                    break;
                }
            }
        }

        // 处理禁止的URL
        if (forbidden) {
            if (level >= 1) {
                printf("[Warning] Forbidden url: %s\n", url);
            }

            char errorResponse[LEN] = {0};
            int errorLen = CreateErrorResponse(buf, length, errorResponse);
            int sendLength = sendto(my_socket, errorResponse, errorLen, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
            if (level >= 1) {
                printf("\n[Send] Time=%d Send Warning to Client. ID=%d Url=%s\n", timeCircle, id, url);
            }
        }

        // 处理A记录查询和非禁止的URL
        if (isFirstQueryTypeA(buf) && !forbidden) {
            ipCount = findIpsAndRefresh(cache, url, curIPs);

            if (ipCount > 0) {
                flag = 1;
            }

            // 如果缓存中未找到，则在本地哈希表中查找
            if (!flag) {
                ipCount = findIPlocallyMultiple(url, hashTable, curIPs, &localIpCount);
                if (ipCount > 0) {
                    if (level >= 2) {
                        printf("[Local Found] Time=%d url=%s IP count=%d. First IP=%s\n", timeCircle, url, ipCount, curIPs[0]);
                    }
                    flag = 1;
                    addCache(cache, url, curIPs, ipCount);
                }
            }

            // 如果找到IP，则发送响应
            if (flag) {
                char converBuf[LEN] = { 0 };
                int respLen = CreateResponse(buf, length, curIPs, ipCount, converBuf);
                int sendLength = sendto(my_socket, converBuf, respLen, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
                if (level >= 1) {
                    printf("\n[Send] Time=%d Send to Client. ID=%d Url=%s\n", timeCircle, id, url);
                }
            }
        }

        // 如果在缓存和本地哈希表中未找到，则将请求转发给服务器
        if (!flag && !forbidden) {
            unsigned short transID = IDFromClientToServer(ID_table, buf, length, id, client_addr, my_socket);

            if (transID != -1) {
                int sendLength = sendto(my_socket, buf, length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
                if (level >= 1) {
                    printf("\n[Send] Time=%d Send to Server. ID=%d Url=%s\n", timeCircle, transID, url);
                }
            } else {
                if (level >= 1) {
                    printf("[Warning] Can't find space in ID_table, throw away.\n");
                }
            }
        }
    } else {
        if (level >= 1) {
            printf("[Warning] Empty packet. Discard.\n");
        }
    }
}

// 设置命令行参数
void set_commandInfo(int argc, char* argv[], char* server_ip, char* file_name) {
    printf("\n\t---DNS RELAY DESIGNRD BY TRENCHANCE---\n");
	printf("\tcommand format: dnsrelay [-d|-dd] [dns-server-ipaddr] [filename]\n\n");
    // 设置调试级别
    level = 0;
    if (argc >= 2) {
        if (strcmp(argv[1], "-d") == 0)
            level = 1;
        else if (strcmp(argv[1], "-dd") == 0)
            level = 2;
        printf("[OK] Debug Level: %d\n", level);
    }
    // 设置服务器IP
    if (argc >= 3) {
        strcpy(server_ip, argv[2]);
        printf("[OK] DNS server: %s\n", server_ip);
    }
    // 设置文件名
    if (argc >= 4) {
        strcpy(file_name, argv[3]);
        printf("[OK] Relay Table File Path: %s\n", file_name);
    }
}

// 初始化套接字
void initialize_socket(char* server_ip) {
    // 创建IPv4的UDP套接字
    my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // 创建套接字失败，重复尝试
    while (my_socket < 0) {
        printf("[Error] Socket build falied!\n");
        my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }
    printf("[OK] Socket build successfully! Socket: %d\n", my_socket);

    client_addr.sin_family = AF_INET;           // IPv4
    client_addr.sin_addr.s_addr = INADDR_ANY;   // 本地ip地址随机
    #ifdef _WIN32
        client_addr.sin_port = htons(53);           // Windows 绑定到 53 端口
    #else
        client_addr.sin_port = htons(5053);         // Linux 绑定到 5053 端口
    #endif

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip); // 绑定到外部服务器
    server_addr.sin_port = htons(53);

    // 设置端口复用
    int reuse = 0;
    setsockopt(my_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    // 将端口号与套接字关联
    int resp = bind(my_socket, (struct sockaddr*)&client_addr, sizeof(client_addr));
    while (resp < 0) {
        printf("[Error] Bind socket port failed!\n");
        resp = bind(my_socket, (struct sockaddr*)&client_addr, sizeof(client_addr));
    }
    printf("[OK] Bind socket port successfully!\n");
}

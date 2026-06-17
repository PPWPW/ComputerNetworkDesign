/**
 * ============================================================
 * 文件：Main.c
 * 功能：DNS 中继服务器的主程序
 * ============================================================
 *
 * 系统架构总览：
 * ┌─────────┐         ┌─────────────────┐         ┌──────────┐
 * │ 客户端  │ ────→    │  DNS 中继服务器 │ ────→   │ 上游 DNS  │
 * │ (浏览器 │ ←────    │  (本程序)       │ ←────   │  服务器   │
 * └─────────┘         └─────────────────┘         └──────────┘
 *                             │
 *                       ┌─────┴─────┐
 *                       │dnsrelay.txt│  (本地域名映射文件)
 *                       └───────────┘
 *
 * 请求处理流程（handle_client_packet）：
 *   客户端查询 → 检查屏蔽列表(0.0.0.0) → 查LRU缓存 → 查本地哈希表
 *   → 若命中则构造响应返回给客户端
 *   → 若未命中则转换ID后转发给上游DNS
 *
 * 响应处理流程（handle_server_packet）：
 *   收到上游响应 → 查ID表恢复原始客户端ID和地址 →
 *   提取IP地址 → 更新LRU缓存和本地哈希表 → 转发给客户端
 *
 * 命令行参数：
 *   dnsrelay [-d|-dd] [dns-server-ipaddr] [filename]
 *     -d     : 调试等级 1 (基本日志)
 *     -dd    : 调试等级 2 (详细日志，含缓存/哈希表操作)
 *     ipaddr : 上行 DNS 服务器 IP (默认 10.3.9.6)
 *     filename: 本地域名映射文件 (默认 dnsrelay.txt)
 * ============================================================
 */

#include "main.h"

 /**
  *main-程序入口
  *
  *初始化:
  *1.解析命令行参数
  *2.分配/初始化ID表，缓存，文件句柄，哈希表
  *3.创建UDP套接字并绑定端口
  *4.启动计时线程
  *
  *主循环：
  *1.检查并处理超时记录
  *2.清空缓冲区，接受数据包
  *3.解析QR标志位
  *
  */
int main(int argc, char* argv[]) {

    //默认上行DNS服务器IP(校园网络IP)
    char server_ip[IPMAXSIZE] = "10.3.9.6";
    //默认域名映射文件
    char file_name[200] = "dnsrelay.txt";

    ID_Table* ID_table;
    Cache* cache;
    FILE* dnsFile;
    HashTable* hashTable;

    //初始化组件
    initialize_all(argc, argv, server_ip, file_name, &ID_table, &cache, &dnsFile, &hashTable);

    char buf[LEN] = { 0 };

    while (1) {

        findOutOfTime(ID_table, my_socket);
        memset(buf, "\0", LEN);

        int length = recvfrom(my_socket, buf, sizeof(buf), 0,
            (struct sockaddr*)&tmp_sockaddr, &sockaddr_in_size);

        HEADER* p = (struct HEADER*)buf;

        if (p->qr == 1) {    //QR=1：服务器 → 中继 → 客户端
            handle_server_packet(buf, length, ID_table, cache, dnsFile, hashTable);
        }
        else if(p->qr==0){   //QR=0：客户端 → 中继 → 服务器
            client_addr = tmp_sockaddr;
            handle_client_packet(buf, length, ID_table, cache, hashTable);
        }

    }

    close(my_socket);

}

/**
 *initialize_all - 初始化程序
 *
 *1.解析命令行参数 → 设置 server_ip、file_name、调试等级 level
 *2.分配并初始化ID转换表
 *3.分配并初始化LRU缓存
 *4.打开域名映射文件，构建本地哈希表
 *5.创建UDP套接字并绑定端口
 *6.启动计时线程（timePass），每秒递增timeCircle
 *
 */

void initialize_all(int argc, char* argv[], char* server_ip, char* file_name,
    ID_Table** ID_table, Cache** cache, FILE** dnsFile, HashTable** hashTable) {
    //解析命令行参数
    set_commandInfo(argc, argv, server_ip, file_name);

    //初始化ID转换表
    *ID_table = (ID_Table*)malloc(sizeof(ID_Table));  // 分配 ID_Table 结构体
    initializeTableID(*ID_table);                      // 初始化记录数组和字段

    //初始化LRU缓存
    *cache = initCache();  // 容量30，含双向链表和哈希表

    //初始化本地哈希表
    *dnsFile = fopen(file_name, "r+");
    if (*dnsFile == NULL) {
        printf("[Error] %s not exist!\n", file_name);
    }
    else {
        *hashTable = initHashTable();
        buildHashTableFromFile(*dnsFile, *hashTable);
    }

#ifdef _WIN32
    WSADATA wsadata;
    WSAData(MAKEWORD(2, 2), &wsadata);
#endif
    
    // 创建UDP套接字、绑定端口、设置上行DNS地址
    initialize_socket(server_ip);

    //计时线程
#ifdef _WIN32
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, timePass, NULL, 0, NULL);
#else 
    pthread_t hThread;
    pthread_create(&hThread, timePass, NULL);
#endif

}

/**
 *handle_server_packet - 处理来自上游DNS服务器的响应
 *
 *处理流程：
 *1.从包头提取DNS ID
 *2.解析域名（用于日志和后续更新）
 *3.通过ID表恢复原始客户端ID和客户端地址
 *4.替换包头ID为原始客户端ID
 *5.通过 UDP 转发给原始客户端
 *6.如果是 A 记录查询，提取IP地址并：
 *   更新 LRU 缓存
 *   追加到本地映射文件
 *   插入本地哈希表
 *
 */
void handle_server_packet(char *buf, int length, ID_Table *ID_table, Cache *cache,
    FILE* dnsFile, HashTable* hashTable) {
    
    unsigned short id = ntohs(((HEADER*)buf)->id);
    char url[URLMAXSIZE];
    char* curIPs[100];
    int ipCount = 0;

    parseDomainFromDnsPacket(buf, url);
    if (level >= 1) {
        printf("\n[Receive] Time=%d Received from Server. TypeA=%d ID=%d Url=%s\n",
            timeCircle, isFirstQueryTypeA(buf), id, url);
    }

    // 通过transID查找ID表中的记录，恢复客户端原始信息
    ID_Table_Record* req = IDFromServerToClient(ID_table, id);

    if (req != NULL) {

        //恢复客户端ID
        id = req->Question_id;
        unsigned short packetID = htons(id);
        memcpy(buf, &packetID, sizeof(unsigned short));

        //转发响应
        int sendLength = sendto(my_socket, buf, length, 0,
            (struct sockaddr*)&req->client_addr,
            sizeof(req->client_addr));
        if (level >= 1) {
            printf("\n[Send] Time=%d Send to Client. ID=%d Url=%s\n", timeCircle, id, url);
        }

        // 如果是 A 记录查询，提取IP并更新缓存和本地表
        if (isFirstQueryTypeA(buf)) {
            extractIpsFromDnsPacket(buf, length, curIPs, &ipCount);

            if (curIPs[0] != NULL) {
                // 更新LRU缓存
                addCache(cache, url, curIPs, ipCount);

                // 持久化到文件并更新本地哈希表
                if (dnsFile != NULL) {
                    for (int i = 0; i < ipCount; i++) {
                        addEntryToRelayTable(dnsFile, curIPs[i], url);
                        insertHashTable(hashTable, curIPs[i], url);
                    }
                }
            }
        }
    }
    else {
        //ID无效
        printf("[Warning] Time:%d Packet ID:%d from Server has Invalid ID (May be Timeout), Discard\n", timeCircle, id);
    }        
}

/**
 *handle_client_packet - 处理来自客户端的DNS查询
 *
 *多级查询策略
 *
 *第1级：屏蔽检查
 *在本地哈希表中查找域名，检查是否有IP = 0.0.0.0
 *如果有，返回NXDOMAIN错误（域名被屏蔽）
 *
 *第2级LRU缓存查找
 *检查最近查询过的域名缓存
 *命中则直接构造响应返回
 *
 *第3级：本地哈希表查找
 *查找从文件和运行时学习到的映射
 *命中则构造响应,同时更新LRU缓存
 *
 *第4级：转发到上游DNS
 *前三级均未命中，通过ID转换表分配transID
 *将查询报文转发给上游DNS服务
 *
 *特殊处理：
 *非A记录查询：跳过本地查询，直接转发
 *空包/过短包：丢弃
 *
 *
 */
void handle_client_packet(char *buf, int length, ID_Table *ID_table, Cache *cache,
    HashTable* hashTable) {
    
    unsigned short id = ntohs(((HEADER*)buf)->id);  // 客户端 DNS ID
    char url[URLMAXSIZE];
    char* curIPs[100];
    int ipCount = 0;

    parseDomainFromDnsPacket(buf, url);
    if (level >= 1) {
        printf("\n[Receive] Time=%d Received from Client. TypeA=%d ID=%d Url=%s\n",
            timeCircle, isFirstQueryTypeA(buf), id, url);
    }
    if (url != NULL && length >= 12) {

        int flag = 0;        // 标记：是否已在本地找到IP(0=未找到，1=找到）
        int forbidden = 0;   // 标记：域名是否被屏蔽

        //第1级屏蔽检查
        char* localIps[100];               // 本地哈希表查到的IP
        int localIpCount = 0;
        memset(localIps, 0, sizeof(localIps));  // 清零指针数组
        findIPlocallyMultiple(url, hashTable, localIps, &localIpCount);

        // 检查本地表结果中是否包含 0.0.0.0
        if (localIpCount > 0) {
            for (int i = 0; i < localIpCount; i++) {
                if (strcmp(localIps[i], "0.0.0.0") == 0) {
                    forbidden = 1;  // 该域名被屏蔽
                    break;
                }
            }
        }

        //处理被屏蔽域名
        if (forbidden) {
            if (level >= 1) {
                printf("[Warning] Forbidden url: %s\n", url);
            }

            // 构造NXDOMAIN错误响应并发送给客户端
            char errorResponse[LEN] = {0};
            int errorLen = CreateErrorResponse(buf, length, errorResponse);
            int sendLength = sendto(my_socket, errorResponse, errorLen, 0,
                                    (struct sockaddr*)&client_addr, sizeof(client_addr));
            if (level >= 1) {
                printf("\n[Send] Time=%d Send Warning to Client. ID=%d Url=%s\n",
                    timeCircle, id, url);
            }
        }

        //第2, 3级：仅对A记录查询且未被屏蔽的域名
        if (isFirstQueryTypeA(buf) && !forbidden) {

            //第2级查找LRU缓存
            ipCount = findIpsAndRefresh(cache, url, curIPs);
            if (ipCount > 0) {
                flag = 1;  // 缓存命中
            }

            //第3级查找本地哈希表
            if (!flag) {  // 缓存未命中
                ipCount = findIPlocallyMultiple(url, hashTable, curIPs, &localIpCount);
                if (ipCount > 0) {
                    if (level >= 2) {
                        printf("[Local Found] Time=%d url=%s IP count=%d. First IP=%s\n",
                            timeCircle, url, ipCount, curIPs[0]);
                    }
                    flag = 1;
                    // 将本地表的结果同步到缓存
                    addCache(cache, url, curIPs, ipCount);
                }
            }

            //如果在缓存或本地表中找到，构造响应发送给客户端
            if (flag) {
                char converBuf[LEN] = { 0 };
                // 构造 DNS 响应
                int respLen = CreateResponse(buf, length, curIPs, ipCount, converBuf);
                int sendLength = sendto(my_socket, converBuf, respLen, 0,
                    (struct sockaddr*)&client_addr, sizeof(client_addr));
                if (level >= 1) {
                    printf("\n[Send] Time=%d Send to Client. ID=%d Url=%s\n",
                        timeCircle, id, url);
                }
            }
        }

        //第4级转发到上游DNS服务器
        if (!flag && !forbidden) {
            // 分配ID表槽位，将客户端查询信息存入ID表
            // 报文的DNS ID 被替换为transID
            unsigned short transID = IDFromClientToServer(ID_table, buf, length,
                id, client_addr, my_socket);

            if (transID != -1) {  // 分配成功
                // 将 ID 替换过的报文转发给上游DNS服务器
                int sendLength = sendto(my_socket, buf, length, 0,
                    (struct sockaddr*)&server_addr, sizeof(server_addr));
                if (level >= 1) {
                    printf("\n[Send] Time=%d Send to Server. ID=%d Url=%s\n",
                        timeCircle, transID, url);
                }
            }
            else {
                // 分配失败,查询被静默丢弃，客户端将超时
                if (level >= 1) {
                    printf("[Warning] Can't find space in ID_table, throw away.\n");
                }
            }
        }
    }
    else {
        // 空包或过短包，丢弃
        if (level >= 1) {
            printf("[Warning] Empty packet. Discard.\n");
        }
    }
}

/**
 *set_commandInfo - 解析命令行参数
 *
 *支持的参数格式：
 *dnsrelay [-d|-dd] [dns-server-ipaddr] [filename]
 *
 *参数位置规则：
 *argv[1]: 调试等级  (-d 或 -dd)
 *argv[2]: DNS 服务器 IP 地址
 *argv[3]: 域名映射文件名
 *
 *
 */

void set_commandInfo(int argc, char* argv[], char* server_ip, char* file_name) {

    //打印程序横幅
    printf("\n\t---DNS RELAY DESIGNRD BY TRENCHANCE---\n");
    printf("\tcommand format: dnsrelay [-d|-dd] [dns-server-ipaddr] [filename]\n\n");

    //解析调试等级
    level = 0;  // 默认不输出调试信息
    if (argc >= 2) {                      // 至少有一个额外参数
        if (strcmp(argv[1], "-d") == 0)
            level = 1;                    // -d: 基本日志（收发/警告）
        else if (strcmp(argv[1], "-dd") == 0)
            level = 2;                    // -dd: 详细日志（含缓存/哈希表操作）
        printf("[OK] Debug Level: %d\n", level);
    }

    //解析上行DNS服务器IP
    if (argc >= 3) {                      // 至少有两个额外参数
        strcpy(server_ip, argv[2]);       // 覆盖默认值 "10.3.9.6"
        printf("[OK] DNS server: %s\n", server_ip);
    }

    //解析域名映射文件名
    if (argc >= 4) {                      // 至少有三个额外参数
        strcpy(file_name, argv[3]);       // 覆盖默认值 "dnsrelay.txt"
        printf("[OK] Relay Table File Path: %s\n", file_name);
    }

}

/**
 * initialize_socket - 创建并绑定UDP套接字
 *
 *流程：
 *1.创建IPv4 UDP套接字 (AF_INET + SOCK_DGRAM + IPPROTO_UDP)
 *2.失败则重试直到成功
 *3.配置本机地址（INADDR_ANY，端口 53/5053）
 *4.配置上行DNS服务器地址（命令行指定或默认 10.3.9.6:53）
 *5.设置 SO_REUSEADDR 选项（允许端口复用）
 *6.bind 绑定端口，失败则重试直到成功
 *
 *端口说明：
 *Windows: 绑定53端口（DNS标准端口）
 *Linux:   绑定5053端口（非root用户无法绑定1024以下端口）
 *
 */
void initialize_socket(char* server_ip) {

    //创建UDP套接字
    my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // 创建失败则反复重试
    while (my_socket < 0) {
        printf("[Error] Socket build falied!\n");
        my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }
    printf("[OK] Socket build successfully! Socket: %d\n", my_socket);

    //配置本机地址
    client_addr.sin_family = AF_INET;        // IPv4
    client_addr.sin_addr.s_addr = INADDR_ANY; // 绑定到所有网卡,接收来自任意接口的报文
#ifdef _WIN32
    client_addr.sin_port = htons(53);        // Windows: DNS 标准端口 53
#else
    client_addr.sin_port = htons(5053);      // Linux: 非特权端口 5053
#endif

    //配置上行DNS服务器地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(53);

    //设置端口复用
    //允许多个套接字绑定同一端口
    int reuse = 0;
    setsockopt(my_socket, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&reuse, sizeof(reuse));

    //将套接字绑定到本机地址和端口
    int resp = bind(my_socket, (struct sockaddr*)&client_addr, sizeof(client_addr));
    while (resp < 0) {
        printf("[Error] Bind socket port failed!\n");
        resp = bind(my_socket, (struct sockaddr*)&client_addr, sizeof(client_addr));
    }
    printf("[OK] Bind socket port successfully!\n");
}

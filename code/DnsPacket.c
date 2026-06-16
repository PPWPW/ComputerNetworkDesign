#include"DnsPacket.h"


void parseDomainFromDnsPacket(char *dnsPacket, char *urlInDns) {
	int currentPosition = 12, urlIndex = 0;
	while (dnsPacket[currentPosition] != 0)
	{
		int segmentLength = dnsPacket[currentPosition];
		for (int j = 0; j < segmentLength; j++)
		{
			urlInDns[urlIndex] = dnsPacket[currentPosition + j + 1];
			urlIndex++;
		}
		urlInDns[urlIndex] = '.';
		urlIndex++;
		currentPosition = currentPosition + segmentLength + 1;
	}
	urlInDns[urlIndex-1] = 0;
}

// 多个IP提取
void extractIpsFromDnsPacket(char *dnsPacket, int packetLength, char* ipStrings[], int *ipCount) {
    int currentPosition = 12;
    int queryCount, answerCount;
    *ipCount = 0;

    queryCount = (dnsPacket[4] << 8) | dnsPacket[5];  // 从高字节和低字节读取 queryCount
    answerCount = (dnsPacket[6] << 8) | dnsPacket[7];  // 从高字节和低字节读取 answerCount

    // 跳过查询部分
    for (int q = 0; q < queryCount; q++) {
        // 跳过域名
        while (dnsPacket[currentPosition] != 0) {
            currentPosition += dnsPacket[currentPosition] + 1;
        }
        currentPosition += 1; // 跳过域名的终止符
        currentPosition += 4; // 跳过查询类型和查询类
    }

    // 遍历回答部分，寻找 A 记录，解析IP地址
    for (int a = 0; a < answerCount; a++) {
        // 跳过 Name 字段
        if ((dnsPacket[currentPosition] & 0xC0) == 0xC0) {
            // 压缩指针
            currentPosition += 2;
        } else {
            while (dnsPacket[currentPosition] != 0) {
                currentPosition += dnsPacket[currentPosition] + 1;
            }
            currentPosition += 1;
        }

        // 跳过 Type 和 Class
        currentPosition += 4;

        // 跳过 TTL
        currentPosition += 4;

        // 获取 Data length
        unsigned short dataLength = (dnsPacket[currentPosition] << 8) | dnsPacket[currentPosition + 1];
        currentPosition += 2;

        // 检查是否为 A 记录且长度为 4
        if (dataLength == 4 && ((dnsPacket[currentPosition - 10] << 8 | dnsPacket[currentPosition - 9]) == 1)) {
            // 分配内存给 ipStrings[*ipCount]
            ipStrings[*ipCount] = (char*)malloc(IPMAXSIZE * sizeof(char));
            if (ipStrings[*ipCount] == NULL) {
                return ;
            }

            // Convert IP address to string
            struct in_addr ipAddr;
            memcpy(&ipAddr, &dnsPacket[currentPosition], sizeof(ipAddr));
            inet_ntop(AF_INET, &ipAddr, ipStrings[*ipCount], INET_ADDRSTRLEN);
            (*ipCount)++;
        }
        currentPosition += dataLength;
    }

    // 确保提取的 IP 不为空
    if (*ipCount == 0) {
        ipStrings = NULL;
    }
}


// Helper function to convert string IP address to network byte order
unsigned long ipToNetworkByteOrder(const char *ip) {
    struct in_addr addr;
    inet_pton(AF_INET, ip, &addr);
    return addr.s_addr;
}


int CreateResponse(char *DnsInfo, int DnsLength, char **FindIps, int ipCount, char *DnsResponse) {
    DnsLength = GetLengthOfDns(DnsInfo);
    // 复制原始报文到响应中
    memcpy(DnsResponse, DnsInfo, DnsLength);

    // 检查是否有IP为 "0.0.0.0"
    int isForbidden = 0;
    for (int i = 0; i < ipCount; i++) {
        if (strcmp(FindIps[i], "0.0.0.0") == 0) {
            isForbidden = 1;
            break;
        }
    }

    // 设置标志位和响应数
    unsigned short responseFlag = isForbidden ? htons(0x8183) : htons(0x8180);
    unsigned short responseCount = isForbidden ? htons(0x0000) : htons(ipCount);

    // 修改标志位和响应数
    memcpy(&DnsResponse[2], &responseFlag, sizeof(responseFlag));
    memcpy(&DnsResponse[6], &responseCount, sizeof(responseCount));

    // 如果 IP 地址为 0.0.0.0，则返回原始报文长度
    if (isForbidden) {
        return DnsLength;
    }

    // 构建回答部分
    int curLen = DnsLength;
    for (int i = 0; i < ipCount; i++) {
        unsigned short Name = htons(0xc00c);
        unsigned short TypeA = htons(0x0001);
        unsigned short ClassA = htons(0x0001);
        unsigned long timeLive = htonl(123); // TTL = 123 秒
        unsigned short IPLen = htons(4); // IP 地址长度
        unsigned long IP = ipToNetworkByteOrder(FindIps[i]);

        char answer[16];

        memcpy(answer, &Name, sizeof(Name));
        memcpy(answer + 2, &TypeA, sizeof(TypeA));
        memcpy(answer + 4, &ClassA, sizeof(ClassA));
        memcpy(answer + 6, &timeLive, sizeof(timeLive));
        memcpy(answer + 10, &IPLen, sizeof(IPLen));
        memcpy(answer + 12, &IP, sizeof(IP));

        // 添加回答部分到响应中
        memcpy(DnsResponse + curLen, answer, sizeof(answer));
        curLen += sizeof(answer);
    }

    return curLen; // 返回响应的长度
}


// 函数：检查第一个查询的类型是否为A记录
int isFirstQueryTypeA(const char *buf) {
    // DNS头部的大小为12字节
    int headerSize = 12;
    int offset = headerSize;

    // 跳过查询部分的域名
    while (buf[offset] != 0) {
        offset += buf[offset] + 1;
    }
    offset += 1; // 跳过域名结束的0字节

    // 获取查询的类型字段
    unsigned short queryType;
    memcpy(&queryType, &buf[offset], sizeof(queryType));
    queryType = ntohs(queryType);

    // 检查查询类型是否为A记录（1）
    if (queryType == 1) {
        return TRUE;
    } else {
        return FALSE;
    }
}

int CreateErrorResponse(char* DnsInfo, int DnsLength, char* DnsResponse) {
    int length = GetLengthOfDns(DnsInfo);
    memcpy(DnsResponse, DnsInfo, length);

    // 设置标志位和响应数
    unsigned short responseFlag = htons(0x8183); // rcode = 3, flags表示响应和错误码
    unsigned short questionCount = htons(1);     // 保持问题部分
    unsigned short answerCount = htons(0);
    unsigned short authorityCount = htons(0);
    unsigned short additionalCount = htons(0);

    // 修改标志位和计数
    memcpy(&DnsResponse[2], &responseFlag, sizeof(responseFlag));
    memcpy(&DnsResponse[4], &questionCount, sizeof(questionCount));
    memcpy(&DnsResponse[6], &answerCount, sizeof(answerCount));
    memcpy(&DnsResponse[8], &authorityCount, sizeof(authorityCount));
    memcpy(&DnsResponse[10], &additionalCount, sizeof(additionalCount));

    return length; // 返回响应的长度
}


//得到长度 
int GetLengthOfDns(char *DnsInfo)
{
	int length = 12;
	while (DnsInfo[length] != 0)
	{
		length += DnsInfo[length] + 1;
	}
	length = length + 5;
	return length;
}
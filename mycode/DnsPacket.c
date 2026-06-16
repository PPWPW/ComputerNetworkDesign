#include"DnsPacket.h"

//从 DNS 查询报文中提取域名（URL）
void parseDomainFromDnsPacket(char* dnsPacket, char* urlInDns) {
    int currentposition = 12, urlIndex = 0;
    while (dnsPacket[currentposition] != 0) {  //循环读取
        int segementLength = dnsPacket[currentposition];
        for (int j = 0; j < segementLength; j++) {
            urlInDns[urlIndex] = dnsPacket[currentposition + j + 1];
            urlIndex++;
        }
        //分隔符'.'
        urlInDns[urlIndex] = '.';
        urlIndex++;
        //下一段
        currentposition = currentposition + segementLength + 1;
    }
    urlInDns[urlIndex - 1] = 0;
}

//从 DNS 响应报文中提取所有 IPv4 地址
void extractIpsFromDnsPacket(char* dnsPacket, int packetLength, char* ipStrings[], int* ipCount) {
    int currentPosition = 12;
    int queryCount, answerCount;
    *ipCount = 0;

    queryCount = (dnsPacket[4] << 8) | dnsPacket[5];
    answerCount = (dnsPacket[6] << 8) | dnsPacket[7];

    //跳过查询部分
    for (int q = 0; q < queryCount; q++) {
        while (dnsPacket[currentPosition] != 0) {
            currentPosition += dnsPacket[currentPosition] + 1;
        }
        currentPosition += 1;  // 跳过域名终止符 0x00
		currentPosition += 4;  // 跳过 Type(2B) + Class(2B)
    }
    //遍历回答部分
    for (int a = 0; a < answerCount; a++) {
        //跳过name
        if ((dnsPacket[currentPosition] & 0xc0) == 0xc0) {
            currentPosition += 2;
        }
        else {
            while (dnsPacket[currentPosition] != 0) {
                currentPosition += dnsPacket[currentPosition] + 1;
            }
            currentPosition += 1;
        }

        //跳过Type(2B) + Class(2B)
        currentPosition += 4;
        //跳过 TTL(4B)
        currentPosition += 4;

        //读取data length
        unsigned short dataLength = (dnsPacket[currentPosition] << 8) | dnsPacket[currentPosition + 1];
        currentPosition += 2;

        if (dataLength == 4 && (dnsPacket[currentPosition - 10] << 8 | dnsPacket[currentPosition - 9]) == 1) {
            ipStrings[*ipCount] = (char*)malloc(IPMAXSIZE * sizeof(char));
            if (ipStrings[*ipCount] == NULL) {
                return ;  //内存分配失败
            }
            struct in_addr ipAddr;
            memcpy(&ipAddr, &dnsPacket[currentPosition], sizeof(ipAddr));
            //转换点分十进制
            inet_ntop(AF_INET, &ipAddr, ipStrings[*ipCount], INET_ADDRSTRLEN);
            (*ipCount)++;
        }
        currentPosition += dataLength;
    }

    //未提取到IP
    if (*ipCount == 0) {
        ipStrings = NULL;
    }
}

//点分十进制整数IP字符串转换为网络字节序32位整数（大端序）
unsigned long ipToNetworkByteOrder(const char* ip) {
    struct in_addr addr;
    inet_pton(AF_INET, ip, &addr);
    return addr.s_addr;
}

//构造DNS响应报文
int CreateResponse(char* DnsInfo, int DnsLength, char** FindIps, int ipCount, char* DnsResponse) {
    DnsLength = GetLengthOfDns(DnsInfo);
    memcpy(DnsResponse, DnsInfo, DnsLength);

    //检查是否屏蔽
    int isForbidden = 0;
    for (int i = 0; i < ipCount; i++) {
        if (strcmp(FindIps[i], "0.0.0.0") == 0) {
            isForbidden = 1;
            break;
        }
    }
    // 标志位字段（第 3-4 字节，共 16 位）：
	//   QR(1) | Opcode(4) | AA(1) | TC(1) | RD(1) | RA(1) | Z(3) | RCODE(4)
	//   htons(0x8183) = 1000 0001 1000 0011 → QR=1(响应), RCODE=3(域名不存在)
	//   htons(0x8180) = 1000 0001 1000 0000 → QR=1(响应), RCODE=0(无错误)
    unsigned short responseFlag = isForbidden ? htons(0x8183) : htons(0x8180);
    unsigned short responseCount = isForbidden ? htons(0x0000) : htons(ipCount);

    //修改报文头部，标志位字段，AnswerCount字段
    memcpy(&DnsResponse[2], &responseFlag, sizeof(responseFlag));
    memcpy(&DnsResponse[6], &responseCount, sizeof(responseCount));

    //屏蔽直接返回
    if (isForbidden) {
        return DnsLength;
    }

    //构建回答部分
    int curLen = DnsLength;
    for (int i = 0; i < ipCount; i++) {
        //Name(2B) + Type(2B) + Class(2B) + TTL(4B) + RDLength(2B) + RData(4B) = 16
        unsigned short Name = htons(0xc00c);             //DNS 压缩指针，指向报文偏移 12 处的域名
        unsigned short TypeA = htons(0x0001);            //TypeA=1(A记录)
        unsigned short ClassA = htons(0x0001);           //ClassA=1(IN)
        unsigned short timeLive = htonl(123);             //TTL=123秒
        unsigned short IPLen = htons(4);                 //IP地址长度=4
        unsigned long IP = ipToNetworkByteOrder(FindIps[i]);    // 转网络字节序的 IP

        char answer[16];
        memcpy(answer, &Name, sizeof(Name));
		memcpy(answer + 2, &TypeA, sizeof(TypeA));
		memcpy(answer + 4, &ClassA, sizeof(ClassA));
        memcpy(answer + 6, &timeLive, sizeof(timeLive));
        memcpy(answer + 10, &IPLen, sizeof(IPLen));
		memcpy(answer + 12, &IP, sizeof(IP));

        //16字节回答追加到报文末尾
        memcpy(DnsResponse + curLen, answer, sizeof(answer));
        curLen += sizeof(answer); 
    }
    return curLen;
}

//检测DNS报文第一个查询是否为A记录
int isFirstQueryTypeA(const char* buf) {
    int headerSize = 12;
    int offset = headerSize;
    while (buf[offset] != 0) {
        offset += buf[offset] + 1;
    }
    offset += 1;

    unsigned short queryType;
    memcpy(&queryType, &buf[offset], sizeof(queryType));
    //网络序转本机序
    queryType = ntohs(queryType);

    if (queryType == 1) {
        return TRUE;
    }
    else {
        return FALSE;
    }

}

//构造DNS错误响应报文
int CreateErrorResponse(char* DnsInfo, int DnsLength, char* DnsResponse) {
    int length = GetLengthOfDns(DnsInfo);

    memcpy(DnsResponse, DnsInfo, length);

    //错误响应头部
    unsigned short responseFlag = htons(0x8183);
    unsigned short questionCount = htons(1);
 	unsigned short answerCount    = htons(0);
	unsigned short authorityCount = htons(0);
    unsigned short additionalCount = htons(0);

    memcpy(&DnsResponse[2],  &responseFlag,    sizeof(responseFlag));      // offset 2: Flags
	memcpy(&DnsResponse[4],  &questionCount,   sizeof(questionCount));     // offset 4: QDCount
	memcpy(&DnsResponse[6],  &answerCount,     sizeof(answerCount));       // offset 6: ANCount
	memcpy(&DnsResponse[8],  &authorityCount,  sizeof(authorityCount));    // offset 8: NSCount
	memcpy(&DnsResponse[10], &additionalCount, sizeof(additionalCount));   // offset 10: ARCount

	return length; 
}

//计算 DNS 报文头部 + 第一个问题条目的实际字节长度
int GetLengthOfDns(char* DnsInfo) {
    int length = 12;
    while (DnsInfo[length] != 0) {
        length += DnsInfo[length] + 1;
    }
    //终止符1B，Type 2B, Class 2B
    length = length + 5;

    return length;
}
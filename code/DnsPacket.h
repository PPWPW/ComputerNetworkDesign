#ifndef DNSPACKAGE_H
#define DNSPACKAGE_H

#include"global.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#ifdef _WIN32
	#include<winsock2.h>
	#include <Ws2tcpip.h>
	#pragma comment(lib,"wsock32.lib")
#endif

#ifdef __linux__
	#include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif


void parseDomainFromDnsPacket(char *dnsPacket, char *urlInDns);
void extractIpsFromDnsPacket(char *dnsPacket, int packetLength, char* ipStrings[], int *ipCount) ;
int CreateResponse(char *DnsInfo, int DnsLength, char **FindIps, int ipCount, char *DnsResponse);

unsigned long ipToNetworkByteOrder(const char *ip);
int isFirstQueryTypeA(const char *buf);
int GetLengthOfDns(char *DnsInfo);
int CreateErrorResponse(char* DnsInfo, int DnsLength, char* DnsResponse);

#endif
#ifndef HEADER_H
#define HEADER_H


// 定义DNS报头结构体
typedef struct {
	unsigned id : 16;    /* 查询标识符，16位 */
	unsigned rd : 1;     /* 递归期望标志，1位 */
	unsigned tc : 1;     /* 截断消息标志，1位 */
	unsigned aa : 1;     /* 授权回答标志，1位 */
	unsigned opcode : 4; /* 操作码，4位，表示查询的目的 */
	unsigned qr : 1;     /* 响应标志，1位，0表示查询，1表示响应 */
	unsigned rcode : 4;  /* 响应码，4位 */
	unsigned cd : 1;     /* 禁用验证标志，1位 */
	unsigned ad : 1;     /* 验证数据标志，1位 */
	unsigned z : 1;      /* 保留字段，必须为0，1位 */
	unsigned ra : 1;     /* 递归可用标志，1位 */
	unsigned qdcount : 16;       /* 问题计数，表示查询的问题条目数，16位 */
	unsigned ancount : 16;       /* 回答计数，表示回答的条目数，16位 */
	unsigned nscount : 16;       /* 权威记录计数，表示授权记录的条目数，16位 */
	unsigned arcount : 16;       /* 附加记录计数，表示附加记录的条目数，16位 */
} HEADER;



#endif
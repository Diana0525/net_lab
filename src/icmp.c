#include "icmp.h"
#include "ip.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/**
 * @brief 处理一个收到的数据包
 *        你首先要检查buf长度是否小于icmp头部长度
 *        接着，查看该报文的ICMP类型是否为回显请求，
 *        如果是，则回送一个回显应答（ping应答），需要自行封装应答包。
 * 
 *        应答包封装如下：
 *        首先调用buf_init()函数初始化txbuf，然后封装报头和数据，
 *        数据部分可以拷贝来自接收到的回显请求报文中的数据。
 *        最后将封装好的ICMP报文发送到IP层。  
 * 
 * @param buf 要处理的数据包
 * @param src_ip 源ip地址
 */
void icmp_in(buf_t *buf, uint8_t *src_ip)
{
    // TODO
    icmp_hdr_t *icmp_hdr;
    icmp_hdr_t new_icmp_hdr;
    uint16_t *data16,temp2;
    data16 = (uint16_t*)malloc(buf->len/2);
    // 检查buf长度是否小于icmp头部长度
    // 首先做报头检测，检测报头长度等
    if(buf->len < sizeof(icmp_hdr_t)){
        return;
    }
    // 查看该报文的ICMP类型是否为回显请求
    icmp_hdr = (struct icmp_hdr_t*) buf->data;
    if(icmp_hdr->type != ICMP_TYPE_ECHO_REQUEST){
        return;
    }
    // 是回显请求,回送一个回显应答
    buf_init(&txbuf, buf->len); // 数据来自请求报文，报头与请求报文报头一样
    // 数据部分拷贝回显请求报文中的数据
    new_icmp_hdr.type = ICMP_TYPE_ECHO_REPLY;
    new_icmp_hdr.code = 0;
    new_icmp_hdr.id = icmp_hdr->id; // 标识，用于标识本ICMP进程
    new_icmp_hdr.seq = icmp_hdr->seq; // 序列号，每发送一个回显请求报文便增加1
    // 计算校验和
    new_icmp_hdr.checksum = 0;
    memcpy(txbuf.data, &new_icmp_hdr, sizeof(new_icmp_hdr));
    memcpy(txbuf.data+sizeof(icmp_hdr_t), buf->data+sizeof(icmp_hdr_t), buf->len-sizeof(icmp_hdr_t));
    // 手动将uint8数组转换为uint16型
    for(int i = 0; i < buf->len/2; i++){
        temp2 = (txbuf.data[2*i] << 8)&0xff00;
        data16[i] = temp2 + txbuf.data[2*i+1];
        data16[i] = swap16(data16[i]);
    }
    new_icmp_hdr.checksum = checksum16(data16, buf->len/2);
    // 计算得到校验和后，再次给txbuf.data赋值
    memcpy(txbuf.data, &new_icmp_hdr, sizeof(new_icmp_hdr));
    // 将数据报发出
    ip_out(&txbuf, src_ip, NET_PROTOCOL_ICMP);
}

/**
 * @brief 发送icmp不可达
 *        你需要首先调用buf_init初始化buf，长度为ICMP头部 + IP头部 + 原始IP数据报中的前8字节 
 *        填写ICMP报头首部，类型值为目的不可达
 *        填写校验和
 *        将封装好的ICMP数据报发送到IP层。
 * 
 * @param recv_buf 收到的ip数据包
 * @param src_ip 源ip地址
 * @param code icmp code，协议不可达或端口不可达
 */
void icmp_unreachable(buf_t *recv_buf, uint8_t *src_ip, icmp_code_t code)
{
    // TODO
    // 调用 buf_init 来初始化 txbuf
    icmp_hdr_t icmp_hdr;
    uint16_t *data16,temp2;
    buf_init(&txbuf, ICMP_WRONG_LEN);
    data16 = (uint16_t*)malloc(txbuf.len/2);
    icmp_hdr.type = ICMP_TYPE_UNREACH; // 类型为目的不可达
    icmp_hdr.code = code;
    icmp_hdr.id = 0;
    icmp_hdr.seq = 0; // 标识符和序列号未用，都为0
    icmp_hdr.checksum = 0; // 校验和初始化为0
    memcpy(txbuf.data, &icmp_hdr, sizeof(icmp_hdr_t));
    // 复制IP数据报头部
    memcpy(txbuf.data+sizeof(icmp_hdr_t), recv_buf, IP_HDR_LEN);
    // 复制原始IP数据报中的前8字节 
    memcpy(txbuf.data+sizeof(icmp_hdr_t)+IP_HDR_LEN, recv_buf, 8);
    // 计算校验和
    // 手动将uint8数组转换为uint16型
    for(int i = 0; i < txbuf.len/2; i++){
        temp2 = (txbuf.data[2*i] << 8)&0xff00;
        data16[i] = temp2 + txbuf.data[2*i+1];
        data16[i] = swap16(data16[i]);
    }
    icmp_hdr.checksum = checksum16(data16, txbuf.len/2);
    // 更新后再次赋值
    memcpy(txbuf.data, &icmp_hdr, sizeof(icmp_hdr_t));
    ip_out(&txbuf, src_ip, NET_PROTOCOL_ICMP);
}
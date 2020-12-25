#include "udp.h"
#include "ip.h"
#include "icmp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define UDP_FACK_HEAD_LEN 12
#define UDP_HEAD_LEN 8
/**
 * @brief udp处理程序表
 * 
 */
static udp_entry_t udp_table[UDP_MAX_HANDLER];

/**
 * @brief udp伪校验和计算
 *        1. 你首先调用buf_add_header()添加UDP伪头部
 *        2. 将IP头部拷贝出来，暂存被UDP伪头部覆盖的IP头部
 *        3. 填写UDP伪头部的12字节字段
 *        4. 计算UDP校验和，注意：UDP校验和覆盖了UDP头部、UDP数据和UDP伪头部
 *        5. 再将暂存的IP头部拷贝回来
 *        6. 调用buf_remove_header()函数去掉UDP伪头部
 *        7. 返回计算后的校验和。  
 * 
 * @param buf 要计算的包
 * @param src_ip 源ip地址
 * @param dest_ip 目的ip地址
 * @return uint16_t 伪校验和
 */
static uint16_t udp_checksum(buf_t *buf, uint8_t *src_ip, uint8_t *dest_ip)
{
    // TODO
    ip_hdr_t ip_hdr;
    udp_hdr_t udp_hdr;
    uint16_t data16[BUF_MAX_LEN],checksum,temp;
    // 获取UDP头部
    memcpy(&udp_hdr, buf->data, sizeof(udp_hdr_t));
    // 将IP头部拷贝出来
    memcpy(&ip_hdr, buf->data-IP_HDR_LEN, IP_HDR_LEN); // 传入的buf指向UDP头
    // 增加一个12字节的伪头部
    buf_add_header(buf, UDP_FACK_HEAD_LEN); 
    // 填写UDP伪头部
    memcpy(buf->data, src_ip, NET_IP_LEN); // 填写源IP地址
    memcpy(buf->data+NET_IP_LEN, dest_ip, NET_IP_LEN); // 填写目的IP地址
    buf->data[2*NET_IP_LEN] = 0;
    buf->data[2*NET_IP_LEN+1] = ip_hdr.protocol; // 填写协议号
    memcpy(buf->data+2*NET_IP_LEN+2, &udp_hdr.total_len, 2); // 填写2字节的UDP长度
    // 计算UDP校验和，覆盖了UDP头部、UDP数据和UDP伪头部
    // 将1字节转为2字节一组，累加后进位加到尾部，若还有进位再加一次
    // buf->data中存放着UPD伪头部、UDP头部以及数据部分
    if(buf->len % 2 == 1){ // 数据报是奇数字节，最后用0填充
        buf->data[buf->len] = 0;
        buf->len++;
    }
    for(int i = 0; i < buf->len/2; i++){
        temp = (buf->data[2*i] << 8)&0xff00;
        data16[i] = temp + buf->data[2*i+1];
        data16[i] = swap16(data16[i]);
    }
    checksum = checksum16(data16, buf->len/2); 
    // 去掉伪头部
    buf_remove_header(buf, UDP_FACK_HEAD_LEN);
    // 再将IP头部拷贝回来
    memcpy(buf->data-IP_HDR_LEN, &ip_hdr, IP_HDR_LEN);
    return checksum;
}

/**
 * @brief 处理一个收到的udp数据包
 *        你首先需要检查UDP报头长度
 *        接着计算checksum，步骤如下：
 *          （1）先将UDP首部的checksum缓存起来
 *          （2）再将UDP首都的checksum字段清零
 *          （3）调用udp_checksum()计算UDP校验和
 *          （4）比较计算后的校验和与之前缓存的checksum进行比较，如不相等，则不处理该数据报。
 *       然后，根据该数据报目的端口号查找udp_table，查看是否有对应的处理函数（回调函数）
 *       
 *       如果没有找到，则调用buf_add_header()函数增加IP数据报头部(想一想，此处为什么要增加IP头部？？)
 *       然后调用icmp_unreachable()函数发送一个端口不可达的ICMP差错报文。
 * 
 *       如果能找到，则去掉UDP报头，调用处理函数（回调函数）来做相应处理。
 * 
 * @param buf 要处理的包
 * @param src_ip 源ip地址
 */
void udp_in(buf_t *buf, uint8_t *src_ip)
{
    // TODO
    uint16_t checksum_udp_head,checksum;
    udp_hdr_t *udp_hdr;
    int flag=0; // 标志位，标志能否在udp_table中找到目的端口号
    // 检测报头长度
    if(buf->len < UDP_HEAD_LEN){
        return;
    }
    udp_hdr = (udp_hdr_t*) buf->data;
    // 重新计算checksum
    // 先将UDP首部的checksum缓存起来
    checksum_udp_head = udp_hdr->checksum;
    // 再将UDP首部的checksum字段清零
    udp_hdr->checksum = 0;
    // 调用udp_checksum函数计算UDP校验和
    checksum = udp_checksum(buf, src_ip, net_if_ip);
    // 比对两个校验和，若不相等，则不处理该数据
    if(checksum_udp_head != checksum){
        return;
    }
    udp_hdr->checksum = checksum;
    // 根据UDP数据报中的目的端口号查找udp_table
    // 查看是否有该目的端口号对应的处理函数
    for(int i = 0; i < UDP_MAX_HANDLER; i++){
        if(udp_table[i].port == swap16(udp_hdr->dest_port)){
            flag = 1;
            // 去掉UDP报头
            buf_remove_header(buf, sizeof(udp_hdr_t));
            // 回调函数
            udp_table[i].handler(&udp_table[i], src_ip, udp_hdr->src_port, buf);
            break;
        }
    }
    if(flag == 0){ // 表示没找到
        // 增加IPv4数据报头部
        buf_add_header(buf, IP_HDR_LEN);
        icmp_unreachable(buf, src_ip, ICMP_CODE_PORT_UNREACH);
    }
}

/**
 * @brief 处理一个要发送的数据包
 *        你首先需要调用buf_add_header()函数增加UDP头部长度空间
 *        填充UDP首部字段
 *        调用udp_checksum()函数计算UDP校验和
 *        将封装的UDP数据报发送到IP层。    
 * 
 * @param buf 要处理的包
 * @param src_port 源端口号
 * @param dest_ip 目的ip地址
 * @param dest_port 目的端口号
 */
void udp_out(buf_t *buf, uint16_t src_port, uint8_t *dest_ip, uint16_t dest_port)
{
    // TODO
    udp_hdr_t *udp_hdr;
    buf_add_header(buf, sizeof(udp_hdr_t)); // 增加UDP报头
    // 填充UDP首部字段
    udp_hdr = (udp_hdr_t*) buf->data;
    udp_hdr->src_port = swap16(src_port);
    udp_hdr->dest_port = swap16(dest_port);
    udp_hdr->total_len = swap16(buf->len); // 长度为UDP头部和UDP数据报的总长度
    udp_hdr->checksum = udp_checksum(buf, net_if_ip, dest_ip);
    ip_out(buf, dest_ip, NET_PROTOCOL_UDP);
}

/**
 * @brief 初始化udp协议
 * 
 */
void udp_init()
{
    for (int i = 0; i < UDP_MAX_HANDLER; i++)
        udp_table[i].valid = 0;
}

/**
 * @brief 打开一个udp端口并注册处理程序
 * 
 * @param port 端口号
 * @param handler 处理程序
 * @return int 成功为0，失败为-1
 */
int udp_open(uint16_t port, udp_handler_t handler)
{
    for (int i = 0; i < UDP_MAX_HANDLER; i++) //试图更新
        if (udp_table[i].port == port)
        {
            udp_table[i].handler = handler;
            udp_table[i].valid = 1;
            return 0;
        }

    for (int i = 0; i < UDP_MAX_HANDLER; i++) //试图插入
        if (udp_table[i].valid == 0)
        {
            udp_table[i].handler = handler;
            udp_table[i].port = port;
            udp_table[i].valid = 1;
            return 0;
        }
    return -1;
}

/**
 * @brief 关闭一个udp端口
 * 
 * @param port 端口号
 */
void udp_close(uint16_t port)
{
    for (int i = 0; i < UDP_MAX_HANDLER; i++)
        if (udp_table[i].port == port)
            udp_table[i].valid = 0;
}

/**
 * @brief 发送一个udp包
 * 
 * @param data 要发送的数据
 * @param len 数据长度
 * @param src_port 源端口号
 * @param dest_ip 目的ip地址
 * @param dest_port 目的端口号
 */
void udp_send(uint8_t *data, uint16_t len, uint16_t src_port, uint8_t *dest_ip, uint16_t dest_port)
{
    buf_init(&txbuf, len);
    memcpy(txbuf.data, data, len);
    udp_out(&txbuf, src_port, dest_ip, dest_port);
}
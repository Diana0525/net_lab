#include "ip.h"
#include "arp.h"
#include "icmp.h"
#include "udp.h"
#include <string.h>
#include <stdio.h>
int ip_id=-1;
uint16_t total_len;

/**
 * @brief 处理一个收到的数据包
 *        你首先需要做报头检查，检查项包括：版本号、总长度、首部长度等。
 * 
 *        接着，计算头部校验和，注意：需要先把头部校验和字段缓存起来，再将校验和字段清零，
 *        调用checksum16()函数计算头部检验和，比较计算的结果与之前缓存的校验和是否一致，
 *        如果不一致，则不处理该数据报。
 * 
 *        检查收到的数据包的目的IP地址是否为本机的IP地址，只处理目的IP为本机的数据报。
 * 
 *        检查IP报头的协议字段：
 *        如果是ICMP协议，则去掉IP头部，发送给ICMP协议层处理
 *        如果是UDP协议，则去掉IP头部，发送给UDP协议层处理
 *        如果是本实验中不支持的其他协议，则需要调用icmp_unreachable()函数回送一个ICMP协议不可达的报文。
 *          
 * @param buf 要处理的包
 */
void ip_in(buf_t *buf)
{
    // TODO 
    ip_hdr_t *ip_hdr = (ip_hdr_t*)buf->data;
    uint16_t temp,checksum; // 缓存头部校验和字段
    uint16_t buf16[CHECK_LEN];
    uint16_t temp2;
    // 报头检查
    if(ip_hdr->version != IP_VERSION_4
        || ip_hdr->total_len > UINT16_MAX
        || ip_hdr->hdr_len > 15
    ){
        return;
    }
    temp = ip_hdr->hdr_checksum;
    ip_hdr->hdr_checksum = 0;
    // 手动将uint8数组转换为uint16型
    for(int i = 0; i < CHECK_LEN; i++){
        temp2 = (buf->data[2*i] << 8)&0xff00;
        buf16[i] = temp2 + buf->data[2*i+1];
        buf16[i] = swap16(buf16[i]);
    }
    checksum = checksum16(buf16, ip_hdr->hdr_len*IP_HDR_LEN_PER_BYTE/2);
    if(temp != checksum){ // 如果不一致，则不处理该数据报
        return;
    }
    ip_hdr->hdr_checksum = temp;
    // 检查收到的数据包的目的IP地址是否为本机的IP地址，只处理目的IP为本机的数据报
    if(memcmp(ip_hdr->dest_ip, net_if_ip, NET_IP_LEN) != 0){
        return;
    }
    
    // 检查IP报头的协议字段
    if(ip_hdr->protocol == NET_PROTOCOL_ICMP){
        // 调用 buf_remove_header 去掉 IP 报头
        buf_remove_header(buf, ip_hdr->hdr_len*IP_HDR_LEN_PER_BYTE);
        icmp_in(buf, ip_hdr->src_ip);
    }else if (ip_hdr->protocol == NET_PROTOCOL_UDP)
    {
        // 调用 buf_remove_header 去掉 IP 报头
        buf_remove_header(buf, ip_hdr->hdr_len*IP_HDR_LEN_PER_BYTE);
        udp_in(buf, ip_hdr->src_ip);
    }else{

        icmp_unreachable(buf, ip_hdr->src_ip, ICMP_CODE_PROTOCOL_UNREACH);// 协议不可达
    }
    
}

/**
 * @brief 处理一个要发送的分片
 *        你需要调用buf_add_header增加IP数据报头部缓存空间。
 *        填写IP数据报头部字段。
 *        将checksum字段填0，再调用checksum16()函数计算校验和，并将计算后的结果填写到checksum字段中。
 *        将封装后的IP数据报发送到arp层。
 * 
 * @param buf 要发送的分片
 * @param ip 目标ip地址
 * @param protocol 上层协议
 * @param id 数据包id
 * @param offset 分片offset，必须被8整除
 * @param mf 分片mf标志，是否有下一个分片
 */
void ip_fragment_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol, int id, uint16_t offset, int mf)
{
    // TODO
    ip_hdr_t *ip_hdr;
    // 调用 buf_add_header 增加 IP 数据报头部缓存空间
    buf_add_header(buf, sizeof(ip_hdr_t));
    uint16_t temp,checksum,temp2;
    uint16_t buf16[CHECK_LEN];
    //  填写 IP 数据报头部字段
    ip_hdr = (ip_hdr_t*) buf->data;
    memcpy(ip_hdr->dest_ip, ip, NET_IP_LEN);
    ip_hdr->version = IP_VERSION_4;
    ip_hdr->hdr_len = 5;
    ip_hdr->tos = 0;
    ip_hdr->total_len = swap16(buf->len);
    ip_hdr->protocol = protocol;
    ip_hdr->id = swap16(id);
    ip_hdr->ttl = 64;
    ip_hdr->flags_fragment = offset/IP_HDR_OFFSET_PER_BYTE&0x1fff;
    if(mf == 1){
        temp = ip_hdr->flags_fragment >> 8;
        temp = temp | IP_MORE_FRAGMENT;
        temp = temp << 8;
        ip_hdr->flags_fragment |= temp; 
    }
    ip_hdr->flags_fragment = swap16(ip_hdr->flags_fragment);
    memcpy(ip_hdr->dest_ip, ip, NET_IP_LEN);
    memcpy(ip_hdr->src_ip, net_if_ip, NET_IP_LEN);
    ip_hdr->hdr_checksum = 0;
    // 手动将uint8数组转换为uint16型
    for(int i = 0; i < CHECK_LEN; i++){
        temp2 = (buf->data[2*i] << 8)&0xff00;
        buf16[i] = temp2 + buf->data[2*i+1];
        buf16[i] = swap16(buf16[i]);
    }
    checksum = checksum16(buf16, ip_hdr->hdr_len*IP_HDR_LEN_PER_BYTE/2);
    ip_hdr->hdr_checksum = checksum;
    arp_out(buf, ip, NET_PROTOCOL_IP);
}

/**
 * @brief 处理一个要发送的数据包
 *        你首先需要检查需要发送的IP数据报是否大于以太网帧的最大包长（1500字节 - ip包头长度）。
 *        
 *        如果超过，则需要分片发送。 
 *        分片步骤：
 *        （1）调用buf_init()函数初始化buf，长度为以太网帧的最大包长（1500字节 - ip包头头长度）
 *        （2）将数据报截断，每个截断后的包长度 = 以太网帧的最大包长，调用ip_fragment_out()函数发送出去
 *        （3）如果截断后最后的一个分片小于或等于以太网帧的最大包长，
 *             调用buf_init()函数初始化buf，长度为该分片大小，再调用ip_fragment_out()函数发送出去
 *             注意：最后一个分片的MF = 0
 *    
 *        如果没有超过以太网帧的最大包长，则直接调用调用ip_fragment_out()函数发送出去。
 * 
 * @param buf 要处理的包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    // TODO 
    buf_t ip_buf;
    uint16_t offset=0;
    uint16_t Ethernet_max_len = ETHERNET_MTU-sizeof(ip_hdr_t); // 以太网帧最大包长
    ip_id++;
    //  检查从上层传递下来的数据报包长是否大于以太网帧的最大包长
    if (buf->len > Ethernet_max_len)// 超过以太网帧的最大包长，则需要分片发送
    {
        while(offset+Ethernet_max_len < buf->len){
            
            buf_init(&ip_buf, Ethernet_max_len);
            memcpy(ip_buf.data, buf->data+offset, Ethernet_max_len);
            total_len = Ethernet_max_len;
            ip_fragment_out(&ip_buf, ip, protocol, ip_id, offset, 1);
            offset += Ethernet_max_len;
        }
        total_len = buf->len - offset;
        buf_init(&ip_buf, total_len);
        memcpy(ip_buf.data, buf->data+offset, total_len);
        ip_fragment_out(&ip_buf, ip, protocol, ip_id, offset, 0);
    }
    else
    {
        total_len = buf->len;
        ip_fragment_out(buf, ip, protocol, ip_id, 0, 0);
    }
}

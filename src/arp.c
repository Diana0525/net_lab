#include "arp.h"
#include "utils.h"
#include "ethernet.h"
#include "config.h"
#include <string.h>
#include <stdio.h>
#define ARP_LENGTH 28

/**
 * @brief 初始的arp包
 * 
 */
static const arp_pkt_t arp_init_pkt = {
    .hw_type = swap16(ARP_HW_ETHER),
    .pro_type = swap16(NET_PROTOCOL_IP),
    .hw_len = NET_MAC_LEN,
    .pro_len = NET_IP_LEN,
    .sender_ip = DRIVER_IF_IP,
    .sender_mac = DRIVER_IF_MAC,
    .target_mac = {0}};

/**
 * @brief arp地址转换表
 * 
 */
arp_entry_t arp_table[ARP_MAX_ENTRY];

/**
 * @brief 长度为1的arp分组队列，当等待arp回复时暂存未发送的数据包
 * 
 */
arp_buf_t arp_buf[2]; // 为了让UDP调试工具第一次发送时也能接收到完整的数据包

/**
 * @brief 更新arp表
 *        你首先需要依次轮询检测ARP表中所有的ARP表项是否有超时，如果有超时，则将该表项的状态改为无效。
 *        接着，查看ARP表是否有无效的表项，如果有，则将arp_update()函数传递进来的新的IP、MAC信息插入到表中，
 *        并记录超时时间，更改表项的状态为有效。
 *        如果ARP表中没有无效的表项，则找到超时时间最长的一条表项，
 *        将arp_update()函数传递进来的新的IP、MAC信息替换该表项，并记录超时时间，设置表项的状态为有效。
 * 
 * @param ip ip地址
 * @param mac mac地址
 * @param state 表项的状态
 */
void arp_update(uint8_t *ip, uint8_t *mac, arp_state_t state)
{
    // TODO
    time_t now_time,max; 
    int vaild_flag=0,temp;
    time(&now_time);// 获取当前时间
    // 轮询检查arp_table中所有ARP表项是否有超时
    for (int i = 0; i < ARP_MAX_ENTRY; i++){
        if(now_time - arp_table[i].timeout > ARP_TIMEOUT_SEC){
            arp_table[i].state = ARP_INVALID;
        }
    }
    // 查找ARP表项是否有ARP_INVALID
    for (int i = 0; i < ARP_MAX_ENTRY; i++){
        if(arp_table[i].state == ARP_INVALID){
            //将传递进来的新ip、mac信息插入到表中
            memcpy(arp_table[i].ip, ip, NET_IP_LEN);
            memcpy(arp_table[i].mac, mac, NET_MAC_LEN);
            arp_table[i].state = state;
            arp_table[i].timeout = now_time; // 超时时间戳中保存加入表的时间
            vaild_flag = 1;
            break;
        }
    }
    if(vaild_flag == 0){ // 所有表项都不是ARP_INVALID
        max = arp_table[0].timeout;
        for(int i = 1; i < ARP_MAX_ENTRY; i++){
            if(arp_table[i].timeout<max){
                max = arp_table[i].timeout;
                temp = i;
            }
        }
        // temp中保存的是最大的timeout表项
        memcpy(arp_table[temp].ip, ip, NET_IP_LEN);
        memcpy(arp_table[temp].mac, mac, NET_MAC_LEN);
        arp_table[temp].state = state;
        arp_table[temp].timeout = now_time; // 超时时间戳中保存加入表的时间
    }
}

/**
 * @brief 从arp表中根据ip地址查找mac地址
 * 
 * @param ip 欲转换的ip地址
 * @return uint8_t* mac地址，未找到时为NULL
 */
static uint8_t *arp_lookup(uint8_t *ip)
{
    for (int i = 0; i < ARP_MAX_ENTRY; i++)
        if (arp_table[i].state == ARP_VALID && memcmp(arp_table[i].ip, ip, NET_IP_LEN) == 0)
            return arp_table[i].mac;
    return NULL;
}

/**
 * @brief 发送一个arp请求
 *        你需要调用buf_init对txbuf进行初始化
 *        填写ARP报头，将ARP的opcode设置为ARP_REQUEST，注意大小端转换
 *        将ARP数据报发送到ethernet层
 * 
 * @param target_ip 想要知道的目标的ip地址
 */
static void arp_req(uint8_t *target_ip)
{
    // TODO
    arp_pkt_t arp_pkt_t;
    // ARP报文长度为28
    buf_init(&txbuf, ARP_LENGTH); 
    // 填写ARP报头
    arp_pkt_t = arp_init_pkt;
    memcpy(arp_pkt_t.target_ip, target_ip, NET_IP_LEN);
    arp_pkt_t.opcode = swap16(ARP_REQUEST);
    memcpy(txbuf.data, &arp_pkt_t, sizeof(arp_pkt_t));
    // 调用ethernet_out函数将ARP报文发送出去
    ethernet_out(&txbuf, ether_broadcast_mac, NET_PROTOCOL_ARP);
}

/**
 * @brief 处理一个收到的数据包
 *        你首先需要做报头检查，查看报文是否完整，
 *        检查项包括：硬件类型，协议类型，硬件地址长度，协议地址长度，操作类型
 *        
 *        接着，调用arp_update更新ARP表项
 *        查看arp_buf是否有效，如果有效，则说明ARP分组队列里面有待发送的数据包。
 *        即上一次调用arp_out()发送来自IP层的数据包时，由于没有找到对应的MAC地址进而先发送的ARP request报文
 *        此时，收到了该request的应答报文。然后，根据IP地址来查找ARM表项，如果能找到该IP地址对应的MAC地址，
 *        则将缓存的数据包arp_buf再发送到ethernet层。
 * 
 *        如果arp_buf无效，还需要判断接收到的报文是否为request请求报文，并且，该请求报文的目的IP正好是本机的IP地址，
 *        则认为是请求本机MAC地址的ARP请求报文，则回应一个响应报文（应答报文）。
 *        响应报文：需要调用buf_init初始化一个buf，填写ARP报头，目的IP和目的MAC需要填写为收到的ARP报的源IP和源MAC。
 * 
 * @param buf 要处理的数据包
 */
void arp_in(buf_t *buf)
{
    // TODO
    arp_pkt_t *arp = (arp_pkt_t*)buf->data;
    arp_pkt_t arp_pkt_t;
    uint8_t *get_mac;
    int count_same=0;
    int opcode = swap16(arp->opcode);
    if (arp->hw_type != swap16(ARP_HW_ETHER)
        || arp->pro_type != swap16(NET_PROTOCOL_IP)
        || arp->hw_len != NET_MAC_LEN
        || arp->pro_len != NET_IP_LEN
        || (opcode != ARP_REQUEST && opcode != ARP_REPLY))
    {
        return ;// 报头有误
    }
    arp_update(arp->sender_ip, arp->sender_mac, ARP_VALID);
    if(arp_buf[0].valid || arp_buf[1].valid){// arp_buf有效
        if(arp_buf[0].valid){
            arp_buf[0].valid = 0;
            get_mac = arp_lookup(arp_buf[0].ip);
            if(get_mac != NULL){
                ethernet_out(&arp_buf[0].buf, get_mac, arp_buf[0].protocol);
            }
        }
        if (arp_buf[1].valid)
        {
            arp_buf[1].valid = 0;
            get_mac = arp_lookup(arp_buf[1].ip);
            if(get_mac != NULL){
                ethernet_out(&arp_buf[1].buf, get_mac, arp_buf[1].protocol);
            }
        }
        
    }else // arp_buf无效
    {
        // 接收到的报文为ARP_REQUEST请求报文且请求报文的target_ip是本机的IP
        if(arp->opcode == swap16(ARP_REQUEST) && memcmp(arp->target_ip, net_if_ip, NET_IP_LEN) == 0){
            // 认为是请求本机的MAC地址的ARP请求报文，回应一个响应报文
            buf_init(&txbuf, ARP_LENGTH); 
            // 填写ARP报头
            arp_pkt_t = arp_init_pkt;
            memcpy(arp_pkt_t.target_ip, arp->sender_ip, NET_IP_LEN);
            memcpy(arp_pkt_t.target_mac, arp->sender_mac, NET_MAC_LEN);
            memcpy(arp_pkt_t.sender_ip, arp->target_ip, NET_IP_LEN);
            memcpy(arp_pkt_t.sender_mac, net_if_mac, NET_MAC_LEN);
            arp_pkt_t.opcode = swap16(ARP_REPLY);// ARP响应包
            memcpy(txbuf.data, &arp_pkt_t, sizeof(arp_pkt_t));
            ethernet_out(&txbuf, arp_pkt_t.target_mac, NET_PROTOCOL_ARP);// 响应请求MAC地址的报文
        }
    }
    
}

/**
 * @brief 处理一个要发送的数据包
 *        你需要根据IP地址来查找ARP表
 *        如果能找到该IP地址对应的MAC地址，则将数据报直接发送给ethernet层
 *        如果没有找到对应的MAC地址，则需要先发一个ARP request报文。
 *        注意，需要将来自IP层的数据包缓存到arp_buf中，等待arp_in()能收到ARP request报文的应答报文
 * 
 * @param buf 要处理的数据包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void arp_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    // TODO
    int count_same=0,i,j,temp,flag=0;
    uint8_t *get_mac;
    get_mac = arp_lookup(ip);
    if(get_mac != NULL){// 标志在表中找到MAC地址
        ethernet_out(buf, get_mac, protocol);
    }
    else{// 没有找到对应的MAC地址
        //将来自IP层的数据包缓存到arp_buf的buf中
        for (int i = 0; i < 2; i++){
            if(arp_buf[i].valid == 0){
                memcpy(&arp_buf[i].buf, buf, sizeof(buf_t));
                memcpy(&arp_buf[i].ip, ip, NET_IP_LEN);
                arp_buf[i].protocol = protocol;
                arp_buf[i].valid = 1;
                arp_req(ip);
                break;
            }
        }
        
    }

}

/**
 * @brief 初始化arp协议
 * 
 */
void arp_init()
{
    for (int i = 0; i < ARP_MAX_ENTRY; i++)
        arp_table[i].state = ARP_INVALID;
    for (int i = 0; i < 2; i++){
        arp_buf[i].valid = 0;
    }
    arp_req(net_if_ip); // 发送一个无回报ARP包
}

/* 
 * run this program as root user
 */

#include<stdio.h>
#include<string.h>
#include<malloc.h>
#include<errno.h>

#include<sys/socket.h>
#include<sys/types.h>
#include<sys/ioctl.h>

#include<net/if.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<netinet/if_ether.h>
#include<netinet/udp.h>

#include<linux/if_packet.h>
#include<arpa/inet.h>

struct ifreq ifreq_c, ifreq_i, ifreq_ip; 
// for each ioctl keep diffrent ifreq structure otherwise error may come in sending(sendto)

int sock_raw;
unsigned char *sendbuff;

#define DESTMAC0 0x08
#define DESTMAC1 0x00
#define DESTMAC2 0x27
#define DESTMAC3 0xd5
#define DESTMAC4 0xcf
#define DESTMAC5 0xaf

#define destination_ip 192.168.3.13

int total_len = 0, send_len;

void get_eth_index()
{
    memset(&ifreq_i, 0, sizeof(ifreq_i));
    strncpy(ifreq_i.ifr_name, "eth0", IFNAMSIZ - 1);

    if((ioctl(sock_raw, SIOCGIFINDEX, &ifreq_i)) < 0)
        printf("error in index ioctl reading");

    printf("index=%d\n", ifreq_i.ifr_ifindex);
}

void get_mac()
{
    memset(&ifreq_c, 0, sizeof(ifreq_c));
    strncpy(ifreq_c.ifr_name, "eth0", IFNAMSIZ - 1);

    if((ioctl(sock_raw, SIOCGIFHWADDR, &ifreq_c)) < 0)
        printf("error in SIOCGIFHWADDR ioctl reading");

    printf("Mac= %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n", (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[0]), (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[1]), (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[2]), (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[3]), (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[4]), (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[5]));

    printf("ethernet packaging start ... \n");

    struct ethhdr *eth = (struct ethhdr *)(sendbuff);
    eth->h_source[0] = (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[0]);
    eth->h_source[1] = (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[1]);
    eth->h_source[2] = (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[2]);
    eth->h_source[3] = (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[3]);
    eth->h_source[4] = (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[4]);
    eth->h_source[5] = (unsigned char)(ifreq_c.ifr_hwaddr.sa_data[5]);

    eth->h_dest[0] = DESTMAC0;
    eth->h_dest[1] = DESTMAC1;
    eth->h_dest[2] = DESTMAC2;
    eth->h_dest[3] = DESTMAC3;
    eth->h_dest[4] = DESTMAC4;
    eth->h_dest[5] = DESTMAC5;

    eth->h_proto = htons(ETH_P_IP); //0x800
    printf("ethernet packaging done.\n");

    total_len += sizeof(struct ethhdr);
}

void get_data()
{
    sendbuff[total_len++] =	0xAA;
    sendbuff[total_len++] =	0xBB;
    sendbuff[total_len++] =	0xCC;
    sendbuff[total_len++] =	0xDD;
    sendbuff[total_len++] =	0xEE;

}

void get_udp()
{
    struct udphdr *uh = (struct udphdr *)(sendbuff + sizeof(struct iphdr) + sizeof(struct ethhdr));

    uh->source	= htons(12345);
    uh->dest	= htons(12345);
    uh->check	= 0;

    total_len += sizeof(struct udphdr);
    get_data();
    uh->len		= htons((total_len - sizeof(struct iphdr) - sizeof(struct ethhdr)));
}

unsigned short checksum(unsigned short *buff, int _16bitword)
{
    unsigned long sum;
    for(sum = 0; _16bitword > 0; _16bitword--)
        sum += htons(*(buff)++);
    do
    {
        sum = ((sum >> 16) + (sum & 0xFFFF));
    }
    while(sum & 0xFFFF0000);

    return (~sum);
}


void get_ip(int low8, int high8)
{
    memset(&ifreq_ip, 0, sizeof(ifreq_ip));
    strncpy(ifreq_ip.ifr_name, "eth0", IFNAMSIZ - 1);
    if(ioctl(sock_raw, SIOCGIFADDR, &ifreq_ip) < 0)
    {
        printf("error in SIOCGIFADDR \n");
    }

    // printf("%s\n",inet_ntoa((((struct sockaddr_in*)&(ifreq_ip.ifr_addr))->sin_addr)));

    struct iphdr *iph = (struct iphdr *)(sendbuff + sizeof(struct ethhdr));
    iph->ihl	= 5;
    iph->version = 4;
    iph->tos	= 16;
    iph->id		= htons(high8 + low8);
	// iph->frag_off = low8;
    iph->ttl	= 64;
    iph->protocol = 17;
    iph->saddr	= inet_addr(inet_ntoa((((struct sockaddr_in *) & (ifreq_ip.ifr_addr))->sin_addr)));
    iph->daddr	= inet_addr("destination_ip"); // put destination IP address

    total_len += sizeof(struct iphdr);
    get_udp();

    iph->tot_len = htons(total_len - sizeof(struct ethhdr));
    iph->check	= htons(checksum((unsigned short *)(sendbuff + sizeof(struct ethhdr)), (sizeof(struct iphdr) / 2)));
}

int main(int argc, char *argv[])
{
    // printf("argstr: %s\n", argv[1]);

    // int argstrlen = strlen(argv[1]);
    // printf("argstrlen: %d\n", argstrlen);

	/*
    int j;
    for(j = 0; j < argstrlen; j++)
    {
        printf("Key: %d; Value: %d\n", j, argv[1][j] << 8);
    }
	*/

    sock_raw = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);
    if(sock_raw == -1)
        printf("error in socket");

    sendbuff = (unsigned char *)malloc(64); // increase in case of large data.Here data is --> AA  BB  CC  DD  EE
    memset(sendbuff, 0, 64);

    get_eth_index();  // interface number
    get_mac();
    // get_ip(low8, high8);

    struct sockaddr_ll sadr_ll;
    sadr_ll.sll_ifindex = ifreq_i.ifr_ifindex;
    sadr_ll.sll_halen   = ETH_ALEN;
    sadr_ll.sll_addr[0]  = DESTMAC0;
    sadr_ll.sll_addr[1]  = DESTMAC1;
    sadr_ll.sll_addr[2]  = DESTMAC2;
    sadr_ll.sll_addr[3]  = DESTMAC3;
    sadr_ll.sll_addr[4]  = DESTMAC4;
    sadr_ll.sll_addr[5]  = DESTMAC5;

    printf("sending...\n");

    int i;
    for(i = 0; i < argstrlen; i++)
    {
        get_ip(i, argv[1][i] << 8);
        send_len = sendto(sock_raw, sendbuff, 64, 0, (const struct sockaddr *)&sadr_ll, sizeof(struct sockaddr_ll));
        if(send_len < 0)
        {
            printf("error in sending....sendlen=%d....errno=%d\n", send_len, errno);
            return -1;
        }
    }

    printf("game over...\n");
}

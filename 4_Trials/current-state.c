#include <stdio.h>
#include <stdlib.h>
#include <pcap.h>
#include <netinet/ether.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <arpa/inet.h>

struct sniff_ethernet {
	char ether_dhost[ETHER_ADDR_LEN]; /* Destination host address */
	char ether_shost[ETHER_ADDR_LEN]; /* Source host address */
	short ether_type; /* IP? ARP? RARP? etc */
};

struct sniff_ip {
	char ip_vhl;		/* version << 4 | header length >> 2 */
	char ip_tos;		/* type of service */
	short ip_len;		/* total length */
	short ip_id;		/* identification */
	short ip_off;		/* fragment offset field */
#define IP_RF 0x8000		/* reserved fragment flag */
#define IP_DF 0x4000		/* don't fragment flag */
#define IP_MF 0x2000		/* more fragments flag */
#define IP_OFFMASK 0x1fff	/* mask for fragmenting bits */
	char ip_ttl;		/* time to live */
	char ip_p;		/* protocol */
	short ip_sum;		/* checksum */
	struct in_addr ip_src,ip_dst; /* source and dest address */
};
#define IP_HL(ip)		(((ip)->ip_vhl) & 0x0f)
#define IP_V(ip)		(((ip)->ip_vhl) >> 4)

typedef u_int tcp_seq;

struct sniff_tcp {
	short th_sport;	
	short th_dport;	
	tcp_seq th_seq;		
	tcp_seq th_ack;		
	char th_offx2;	
#define TH_OFF(th)	(((th)->th_offx2 & 0xf0) >> 4)
	char th_flags;
#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20
#define TH_ECE 0x40
#define TH_CWR 0x80
#define TH_FLAGS (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
	short th_win;		
	short th_sum;		
	short th_urp;		
};

#define SIZE_ETHERNET 14

const struct sniff_ethernet *ethernet;
const struct sniff_ip *ip; 
const struct sniff_tcp *tcp; 
const char *payload; 

int size_ip;
int size_tcp;

int targetport;

void packet_handler(u_char *user_data, const struct pcap_pkthdr *pkthdr, const u_char *packet);

void hex_dump(const u_char *packet, int length);


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    pcap_if_t *alldevs;
    pcap_if_t *d;

    char *port_arg = argv[1];
    int targetport = atoi(port_arg);

    pcap_t *handle;
    char errbuf[PCAP_ERRBUF_SIZE];
    char filter_exp[100];  
    
    snprintf(filter_exp, sizeof(filter_exp), "port %d", targetport);

    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        fprintf(stderr, "pcap_findalldevs: %s\n", errbuf);
        return 1;
    }

    d = alldevs;

    printf("Dev step %s.\n", d->name);

    handle = pcap_open_live(d->name, BUFSIZ, 1, 1000, errbuf); 
    if (handle == NULL) {
        perror("Device down");
        return 2;
    }

    //printf("Handle step.");

    if (pcap_datalink(handle) != DLT_EN10MB) {
        perror("Ethernet headers - not supported");
        return 2;
    }

    struct bpf_program fp;
    if (pcap_compile(handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(handle));
        return 2;
    }
    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "Couldn't install filter %s: %s\n", filter_exp, pcap_geterr(handle));
        return 2;
    }

    //printf("I'm here. Filter added.");

    const u_char *packet;
    pcap_loop(handle, 0, packet_handler, (u_char *)handle);
    pcap_close(handle);

    return 0;
}

void packet_handler(u_char *user_data, const struct pcap_pkthdr *pkthdr, const u_char *packet) {
    
    //printf("I'm here 2.");

    struct sniff_ethernet *eth_header = (struct sniff_ethernet *)packet;
    char src_mac[ETHER_ADDR_LEN];
    char dst_mac[ETHER_ADDR_LEN];
    ether_ntoa_r((struct ether_addr *)eth_header->ether_shost, src_mac);
    ether_ntoa_r((struct ether_addr *)eth_header->ether_dhost, dst_mac);

    printf("Source MAC: %s\n", src_mac);
    printf("Destination MAC: %s\n", dst_mac);

    int size_ethernet = SIZE_ETHERNET;

    if (ntohs(eth_header->ether_type) == ETHERTYPE_IP) {
        struct sniff_ip *ip_header = (struct sniff_ip *)(packet + size_ethernet);
        char src_ip[INET_ADDRSTRLEN];
        char dst_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ip_header->ip_src), src_ip, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(ip_header->ip_dst), dst_ip, INET_ADDRSTRLEN);

        printf("Source IP: %s\n", src_ip);
        printf("Destination IP: %s\n", dst_ip);

        int size_ip = IP_HL(ip_header) * 4;

        if (ip_header->ip_p == IPPROTO_TCP) {
            struct sniff_tcp *tcp_header = (struct sniff_tcp *)(packet + size_ethernet + size_ip);
            printf("Source Port: %d\n", ntohs(tcp_header->th_sport));
            printf("Destination Port: %d\n", ntohs(tcp_header->th_dport));
        }

        printf("Hex Dump of the Packet:\n");
        hex_dump(packet, pkthdr->len);
    }

    char decision;
    printf("Do you want to forward this packet? (y/n): ");
    scanf(" %c", &decision);

    if (decision == 'y' || decision == 'Y') {
        pcap_t *handle = (pcap_t *)user_data;

        // Fwd back
        if (pcap_inject(handle, packet, pkthdr->len) == -1) {
            pcap_perror(handle, "pcap_inject");
        }
    }
}

void hex_dump(const u_char *packet, int length) {
    for (int i = 0; i < length; i++) {
        printf("%02X ", packet[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}
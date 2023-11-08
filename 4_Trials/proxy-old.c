#include <stdio.h>
#include <pcap.h>
#include <netinet/ether.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>

void packet_handler(u_char *user_data, const struct pcap_pkthdr *pkthdr, const u_char *packet);

int main() {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;
    pcap_if_t *alldevs;
    pcap_if_t *d;

    // Retrieve a list of available devices
    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        fprintf(stderr, "pcap_findalldevs: %s\n", errbuf);
        return 1;
    }

    // Select the first available device
    if (alldevs == NULL) {
        fprintf(stderr, "No network devices found.\n");
        return 1;
    }
    d = alldevs;

    handle = pcap_open_live(d->name, BUFSIZ, 1, 1000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open device: %s\n", errbuf);
        return 1;
    }

    pcap_loop(handle, 0, packet_handler, (u_char *)handle);

    pcap_close(handle);
    pcap_freealldevs(alldevs);

    return 0;
}

void packet_handler(u_char *user_data, const struct pcap_pkthdr *pkthdr, const u_char *packet) {

    struct ether_header *eth_header = (struct ether_header *)packet;
    char src_mac[ETHER_ADDR_LEN];
    char dst_mac[ETHER_ADDR_LEN];
    ether_ntoa_r((struct ether_addr *)&eth_header->ether_shost, src_mac);
    ether_ntoa_r((struct ether_addr *)&eth_header->ether_dhost, dst_mac);

    printf("Source MAC: %s\n", src_mac);
    printf("Destination MAC: %s\n", dst_mac);

    char decision;
    printf("Do you want to forward this packet? (y/n): ");
    scanf(" %c", &decision);

    if (decision == 'y' || decision == 'Y') {
        pcap_t *handle = (pcap_t *)user_data;

        if (pcap_inject(handle, packet, pkthdr->len) == -1) {
            pcap_perror(handle, "pcap_inject");
        }
    }
}


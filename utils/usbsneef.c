#include <stdio.h>

#include <pcap.h>

static const unsigned char OUTPUT = 0;
static const unsigned char INPUT = 1;
static const unsigned char BULK_TRANSFER = 0x03;
static const unsigned char URB_SUBMITTED = 0x53;
static const unsigned char URB_COMPLETED = 0x43;

int main(int argc, char **argv) {

    if (argc == 1) {
        printf("Usage: %s <PCAPFILE>\n", argv[0]);
        return 0;
    }

    pcap_t *handle;
    char errbuf[PCAP_ERRBUF_SIZE];
    handle = pcap_open_offline(argv[1], errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open device: %s\n", errbuf);
        return(2);
    }

    const u_char *packet;		/* The actual packet */
    struct pcap_pkthdr header;	/* The header that pcap gives us */

    while (packet = pcap_next(handle, &header)) {
        unsigned char packetType = packet[0x08];
        unsigned char endpoint = packet[0x0A]; // 0x80 - control - 0x81 device endpoint 1, 0x83 device endpoint 2 (frames come from here)
        unsigned char dir = OUTPUT;
        if ((endpoint & 0x80) == 0x80) {
            // 0x80 mask means input
            dir = INPUT;
        }
        endpoint &= 0x0f;

        unsigned char transferType = packet[0x09];

        unsigned int dataLength = *((unsigned int *)(packet + 0x24));

        if (transferType != BULK_TRANSFER) {
            continue;
        }

        // ignore packets that are submitted and input, or complete and output as they lack the data we care about
        if ((packetType == URB_SUBMITTED && dir == INPUT) || (packetType == URB_COMPLETED && dir == OUTPUT)) {
            continue;
        }

        // Handle bulk transfer packets now

        if (dir == OUTPUT) {
            printf("OUTPUT ");
            for (int i = 0; i < dataLength; i++) {
                printf("%02X ", packet[0x40 + i]);
            }
            printf("\n");
        } else if (dir == INPUT && endpoint != 3) {
            printf("IN ");
            for (int i = 0; i < dataLength; i++) {
                printf("%02X ", packet[0x40 + i]);
            }
            printf("\n");
        } else if (dir == INPUT) {
            printf("FRAME\n");
        }
    }

    return 0;
}
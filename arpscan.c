#include <pcap.h>
#include <signal.h> 
#include <libnet.h>

#include "utils.h"

libnet_t* ln;
struct libnet_ether_addr* src_hw_addr;
char errbuf_lib_net[LIBNET_ERRBUF_SIZE];

pcap_t* handle;
bpf_u_int32 netp, maskp;
struct bpf_program fp;
char* errbuf;

u_int32_t target_ip_addr, src_ip_addr;
const u_int8_t bcast_hw_addr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
const u_int8_t zero_hw_addr[6]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

struct args parsed_args;

void send_arp_req(char* target_ip)
{
   libnet_clear_packet(ln);
   target_ip_addr = libnet_name2addr4(ln, target_ip, LIBNET_RESOLVE);
   libnet_autobuild_arp(
     ARPOP_REQUEST,                   /* operation type       */
     src_hw_addr->ether_addr_octet,   /* sender hardware addr */
     (u_int8_t*) &src_ip_addr,        /* sender protocol addr */
     zero_hw_addr,                    /* target hardware addr */
     (u_int8_t*) &target_ip_addr,     /* target protocol addr */
     ln);                             /* libnet context       */
   libnet_autobuild_ethernet(
     bcast_hw_addr,                   /* ethernet destination */
     ETHERTYPE_ARP,                   /* ethertype            */
     ln);                             /* libnet context       */
   libnet_write(ln);
}

void cleanup()
{
 pcap_close(handle);
 free(errbuf);
}

void stop(int signo)
{
 pcap_breakloop(handle);
 exit(EXIT_SUCCESS);
}

void trap(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes)
{
   struct arphdr* arp = (struct arphdr*)(bytes + 14);
   if(ntohs(arp->opcode) == 2 && memcmp(arp->target_ip_addr, &src_ip_addr, 4) == 0)
   {
     printf("%d", arp->sender_ip_addr[0]);
     printf(" %d", arp->sender_ip_addr[1]);
     printf(" %d", arp->sender_ip_addr[2]);
     printf(" %d ", arp->sender_ip_addr[3]);

     printf(" %02x", arp->sender_mac_addr[0]);
     printf(" %02x", arp->sender_mac_addr[1]);
     printf(" %02x", arp->sender_mac_addr[2]);
     printf(" %02x", arp->sender_mac_addr[3]);
     printf(" %02x", arp->sender_mac_addr[4]);
     printf(" %02x", arp->sender_mac_addr[5]);
     printf("\n");
   }
}

int main(int argc, char** argv)
{
   parsed_args = parse_args(argc, argv);

   ln = libnet_init(LIBNET_LINK, parsed_args.interface, errbuf_lib_net);
   src_ip_addr = libnet_get_ipaddr4(ln);
   src_hw_addr = libnet_get_hwaddr(ln);
   atexit(cleanup);
   signal(SIGINT, stop);
   errbuf = malloc(PCAP_ERRBUF_SIZE);
   handle = pcap_create(parsed_args.interface, errbuf);
   pcap_set_promisc(handle, 1);
   pcap_set_snaplen(handle, 65535);
   pcap_set_timeout(handle, 1000);
   pcap_activate(handle);
   pcap_lookupnet(parsed_args.interface, &netp, &maskp, errbuf);
   pcap_compile(handle, &fp, "arp", 0, maskp);
   pcap_setfilter(handle, &fp);

   printf("%s", "Starting arp-scan\n");
   if(strcmp(parsed_args.target_type, "localnet") == 0)
   {
      char** hosts = NULL;
      int size = 0;
      struct localnet loc_net = get_localnet_info(parsed_args.interface);
      find_local_hosts(&loc_net, &hosts, &size);
      for(int i = 0; i < size; i++)
      {
         send_arp_req(hosts[i]);
      }

      signal(SIGALRM, stop);
      alarm(10);
      pcap_loop(handle, -1, trap, NULL);
   }
   else
   {
      char host[INET_ADDRSTRLEN];
      strcpy(host, parsed_args.target);
      send_arp_req(host);
      signal(SIGALRM, stop);
      alarm(3);
      pcap_loop(handle, -1, trap, NULL);
   }
   libnet_destroy(ln);
   return EXIT_SUCCESS;
}
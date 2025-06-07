#include <pcap.h>
#include <signal.h> 
#include <libnet.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 1235
#define RESULT_BUF_SIZE 64

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

struct arphdr
{
   u_int16_t ftype;
   u_int16_t ptype;
   u_int8_t  flen;
   u_int8_t  plen;
   u_int16_t opcode;
   u_int8_t  sender_mac_addr[6];
   u_int8_t  sender_ip_addr[4];
   u_int8_t  target_mac_addr[6];
   u_int8_t  target_ip_addr[4];
};

char** results = NULL;
int result_count = 0;
int result_capacity = 0;

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

void add_result(const char* entry)
{
   if(result_count == result_capacity)
   {
      result_capacity = result_capacity == 0 ? 16 : result_capacity * 2;
      results = realloc(results, result_capacity * sizeof(char*));
      if (!results)
      {
          perror("realloc");
          exit(EXIT_FAILURE);
      }
   }

   results[result_count] = strdup(entry);
   if (!results[result_count])
   {
      perror("strdup");
      exit(EXIT_FAILURE);
   }

   result_count++;
}

void cleanup()
{
   pcap_close(handle);
   free(errbuf);
}

void stop(int signo)
{
   pcap_breakloop(handle);
}

void trap(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes)
{
   struct arphdr* arp = (struct arphdr*)(bytes + 14);
   if(ntohs(arp->opcode) == 2 && memcmp(arp->target_ip_addr, &src_ip_addr, 4) == 0)
   {
      char buf[RESULT_BUF_SIZE];
      snprintf(buf, sizeof(buf), "%d.%d.%d.%d %02x:%02x:%02x:%02x:%02x:%02x",
          arp->sender_ip_addr[0], arp->sender_ip_addr[1], arp->sender_ip_addr[2], arp->sender_ip_addr[3],
          arp->sender_mac_addr[0], arp->sender_mac_addr[1], arp->sender_mac_addr[2],
          arp->sender_mac_addr[3], arp->sender_mac_addr[4], arp->sender_mac_addr[5]);
      
      add_result(buf);
   }
}

int main(int argc, char** argv)
{
   ln = libnet_init(LIBNET_LINK, argv[1], errbuf_lib_net);
   src_ip_addr = libnet_get_ipaddr4(ln);
   src_hw_addr = libnet_get_hwaddr(ln);
   atexit(cleanup);
   signal(SIGINT, stop);
   errbuf = malloc(PCAP_ERRBUF_SIZE);
   handle = pcap_create(argv[1], errbuf);
   pcap_set_promisc(handle, 1);
   pcap_set_snaplen(handle, 65535);
   pcap_set_timeout(handle, 1000);
   pcap_activate(handle);
   pcap_lookupnet(argv[1], &netp, &maskp, errbuf);
   pcap_compile(handle, &fp, "arp", 0, maskp);
   pcap_setfilter(handle, &fp);

   int sfd, cfd;
   struct sockaddr_in addr;
   socklen_t addrlen = sizeof(addr);

   sfd = socket(AF_INET, SOCK_STREAM, 0);

   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = INADDR_ANY;
   addr.sin_port = htons(PORT);

   bind(sfd, (struct sockaddr*) &addr, sizeof(addr));
   listen(sfd, 1);

   cfd = accept(sfd, (struct sockaddr*) &addr, &addrlen);
   
   int size;
   recv(cfd, &size, sizeof(int), 0);
   printf("%d\n", size);
   char** hosts = malloc(size * sizeof(char*));
   for(int i = 0; i < size; i++)
   {
      hosts[i] = malloc(INET_ADDRSTRLEN);
      recv(cfd, hosts[i], INET_ADDRSTRLEN, 0);
      printf("%s\n", hosts[i]);
      send_arp_req(hosts[i]);
      free(hosts[i]);
   }
   free(hosts);
   
   signal(SIGALRM, stop);
   alarm(10);
   pcap_loop(handle, -1, trap, NULL);

   send(cfd, &result_count, sizeof(int), 0);
   for (int i = 0; i < result_count; i++)
   {
      send(cfd, results[i], RESULT_BUF_SIZE, 0);
      free(results[i]);
   }
   free(results);
   libnet_destroy(ln);
   return EXIT_SUCCESS;
}
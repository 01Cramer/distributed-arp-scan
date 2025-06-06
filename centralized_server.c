#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#define MAX_NODES 10
#define PORT 1235
#define RESULT_BUF_SIZE 64

struct args_t
{
   char* interface;
   char* target_type; // Localnet only at this moment
   char* nodes[MAX_NODES];

   uint8_t num_nodes;
};

struct localnet_t
{
   uint32_t local_ip;
   uint32_t mask;
   uint32_t network;
   uint32_t broadcast;
};

struct args_t parse_args(int argc, char** argv)
{
    struct args_t ret;
    ret.interface = "null";
    ret.target_type = "null";
    ret.num_nodes = 0;
    argv++;
    argc--;

    while (argc > 0)
    {
        if ((*argv)[0] == '-')
        {
            if (strcmp(*argv, "--interface") == 0 || strcmp(*argv, "-i") == 0)
            {
                argv++;
                argc--;
                if (argc > 0)
                {
                    ret.interface = *argv;
                }
                else
                {
                    fprintf(stderr, "Error: expecting field after --interface\n");
                    exit(0);
                }
            }
            else if (strcmp(*argv, "--localnet") == 0 || strcmp(*argv, "-l") == 0)
            {
                ret.target_type = "localnet";
            }
            else if (strcmp(*argv, "--node") == 0 || strcmp(*argv, "-n") == 0)
            {
                argv++;
                argc--;
                if(argc > 0 && ret.num_nodes < MAX_NODES)
                {
                    ret.nodes[ret.num_nodes] = *argv;
                    ret.num_nodes++;
                }
                else
                {
                    fprintf(stderr, "Error: expecting field after --node\n");
                    exit(0);
                }
            }
            else
            {
                fprintf(stderr, "Warning: unknown field\n");
            }
        }
        argv++;
        argc--;
    }

    if(strcmp(ret.interface, "null") == 0)
    {
        fprintf(stderr, "Error: expecting interface\n");
        exit(0);   
    }
    else if(strcmp(ret.target_type, "null") == 0)
    {
        fprintf(stderr, "Error: expecting target or localnet\n");
        exit(0); 
    }
    else if(ret.num_nodes == 0)
    {
        fprintf(stderr, "Error: expecting at least one node specified");
        exit(0);
    }
    
    return ret;
}

struct localnet_t get_localnet_info(char* interface)
{
    struct localnet_t ret;
    struct sockaddr_in *addr;
    struct ifreq ifr;
    int sock;

    strncpy(ifr.ifr_name, interface, IFNAMSIZ);
    sock = socket(AF_INET,SOCK_DGRAM, 0);
    ioctl(sock, SIOCGIFADDR, &ifr);
    addr = (struct sockaddr_in*)&(ifr.ifr_addr);
    ret.local_ip = addr->sin_addr.s_addr;

    ioctl(sock, SIOCGIFNETMASK, &ifr);
    addr = (struct sockaddr_in*)&(ifr.ifr_addr);
    ret.mask = addr->sin_addr.s_addr;

    ret.network = ret.local_ip & ret.mask;
    ret.broadcast = ret.local_ip | ~ret.mask;

    return ret;
}

const char* ip_to_string(uint32_t ip, char* buffer, size_t size)
{
    struct in_addr addr = { .s_addr = ip };
    return inet_ntop(AF_INET, &addr, buffer, size);
}

void add_host(char*** hosts_array, int* size, char* host)
{
    char** temp = realloc(*hosts_array, (*size + 1) * sizeof(char*));
    *hosts_array = temp;

    (*hosts_array)[*size] = malloc(strlen(host) + 1);

    strcpy((*hosts_array)[*size], host);
    (*size)++;
}

void find_local_hosts(struct localnet_t* loc_net, char*** hosts_array, int* size)
{
    uint32_t network = ntohl(loc_net->network);
    uint32_t broadcast = ntohl(loc_net->broadcast);
    uint32_t ip = network + 1;
    char host[INET_ADDRSTRLEN];
    while(ip < broadcast)
    {
       ip_to_string(htonl(ip), host, sizeof(host));
       add_host(hosts_array, size, host);
       ip++;
    }
}

int main(int argc, char** argv)
{
   struct args_t parsed_args = parse_args(argc, argv);
   if(strcmp(parsed_args.target_type, "localnet") == 0)
   {
        char** hosts = NULL;
        int size = 0;
        struct localnet_t loc_net = get_localnet_info(parsed_args.interface);
        find_local_hosts(&loc_net, &hosts, &size);
        int hosts_per_node = size / parsed_args.num_nodes;
        int rest = size % parsed_args.num_nodes;

        for(int i = 0; i < parsed_args.num_nodes; i++)
        {
            int sfd = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in naddr;

            naddr.sin_family = AF_INET;
            naddr.sin_port = htons(PORT); // IF NOT ON LOCAL HOST REMOVE + i
            inet_pton(AF_INET, parsed_args.nodes[i], &naddr.sin_addr);

            if(connect(sfd, (struct sockaddr*) &naddr, sizeof(naddr)) < 0)
            {
                perror("Connection Error");
                exit(0);
            }

            if(i == parsed_args.num_nodes - 1 && rest != 0){ hosts_per_node += rest; }
            send(sfd, &hosts_per_node, sizeof(int), 0);
            for(int j = 0; j < hosts_per_node; j++)
            {
                int index = (hosts_per_node * i) + j;
                send(sfd, hosts[index], INET_ADDRSTRLEN, 0);
            }

            int result_count;
            recv(sfd, &result_count, sizeof(int), 0);
            for (int i = 0; i < result_count; i++) {
                char result[RESULT_BUF_SIZE];
                memset(result, 0, sizeof(result));
                recv(sfd, result, sizeof(result), 0);
                printf("%s\n", result);
            }
        }
        for(int i = 0; i < size; i++)
        {
            free(hosts[i]);
        }
        free(hosts);
   }
   return EXIT_SUCCESS;
}
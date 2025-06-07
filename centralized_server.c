#include <sys/ioctl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

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

struct thread_args_t
{
    char* node_ip;
    char** hosts;
    int hosts_per_node;
    int rest;
    int node_index;
    int total_nodes;
    int total_hosts;
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
        if((*argv)[0] == '-')
        {
            if(strcmp(*argv, "--interface") == 0 || strcmp(*argv, "-i") == 0)
            {
                argv++;
                argc--;
                if(argc > 0)
                {
                    ret.interface = *argv;
                }
                else
                {
                    fprintf(stderr, "Error: expecting field after --interface\n");
                    exit(0);
                }
            }
            else if(strcmp(*argv, "--localnet") == 0 || strcmp(*argv, "-l") == 0)
            {
                ret.target_type = "localnet";
            }
            else if(strcmp(*argv, "--node") == 0 || strcmp(*argv, "-n") == 0)
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
    
    close(sock);
    
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

void* handle_node(void* args)
{
    struct thread_args_t* targs = (struct thread_args_t*)args;

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in naddr;
    naddr.sin_family = AF_INET;
    naddr.sin_port = htons(PORT);
    inet_pton(AF_INET, targs->node_ip, &naddr.sin_addr);

    if(connect(sfd, (struct sockaddr*) &naddr, sizeof(naddr)) < 0)
    {
        perror("Connection Error");
        free(args);
        close(sfd);
        pthread_exit(NULL);
    }

    int actual_hosts = targs->hosts_per_node;
    if(targs->node_index == targs->total_nodes - 1 && targs->rest != 0)
    {
        actual_hosts += targs->rest;
    }

    send(sfd, &actual_hosts, sizeof(int), 0);
    for(int i = 0; i < actual_hosts; i++)
    {
        int index = (targs->hosts_per_node * targs->node_index) + i;
        send(sfd, targs->hosts[index], INET_ADDRSTRLEN, 0);
    }

    int result_count;
    recv(sfd, &result_count, sizeof(int), 0);
    for (int i = 0; i < result_count; i++) {
        char result[RESULT_BUF_SIZE];
        memset(result, 0, sizeof(result));
        recv(sfd, result, sizeof(result), 0);
        printf("%s\n", result);
    }
    
    close(sfd);
    free(targs);
    pthread_exit(NULL);
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

        pthread_t threads[MAX_NODES];
        for(int i = 0; i < parsed_args.num_nodes; i++)
        {
            struct thread_args_t* targs = malloc(sizeof(struct thread_args_t));
            targs->node_ip = parsed_args.nodes[i];
            targs->hosts = hosts;
            targs->hosts_per_node = hosts_per_node;
            targs->rest = rest;
            targs->node_index = i;
            targs->total_nodes = parsed_args.num_nodes;
            targs->total_hosts = size;

            pthread_create(&threads[i], NULL, handle_node, targs);
        }
        for(int i = 0; i < parsed_args.num_nodes; i++)
        {
            pthread_join(threads[i], NULL);
        }
        for(int i = 0; i < size; i++)
        {
            free(hosts[i]);
        }
        free(hosts);
   }
   return EXIT_SUCCESS;
}
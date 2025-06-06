#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "utils.h"

struct args parse_args(int argc, char** argv)
{
    struct args ret;
    ret.interface = "null";
    ret.target_type = "null";
    ret.target = "null";
    argv++;
    argc--;

    while (argc > 0)
    {
        if ((*argv)[0] == '-')
        {
            if (strcmp(*argv, "--interface") == 0)
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
            else if (strcmp(*argv, "-i") == 0)
            {
                argv++;
                argc--;
                if (argc > 0)
                {
                    ret.interface = *argv;
                }
                else
                {
                    fprintf(stderr, "Error: expecting field after -i\n");
                    exit(0);
                }
            }
            else if (strcmp(*argv, "--target") == 0)
            {
                ret.target_type = "target";
                argv++;
                argc--;
                if (argc > 0)
                {
                    ret.target = *argv;
                }
                else
                {
                    fprintf(stderr, "Error: expecting field after --target\n");
                    exit(0);
                }
            }
            else if (strcmp(*argv, "-t") == 0)
            {
                ret.target_type = "target";
                argv++;
                argc--;
                if (argc > 0)
                {
                    ret.target = *argv;
                }
                else
                {
                    fprintf(stderr, "Error: expecting field after -t\n");
                    exit(0);
                }
            }
            else if (strcmp(*argv, "--localnet") == 0)
            {
                ret.target_type = "localnet";
            }
            else if (strcmp(*argv, "-l") == 0)
            {
                ret.target_type = "localnet";
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
    
    return ret;
}

struct localnet get_localnet_info(char* interface)
{
    struct localnet ret;
    struct sockaddr_in *addr;
    struct ifreq ifr;
    int sockfd;

    strncpy(ifr.ifr_name, interface, IFNAMSIZ);
    sockfd = socket(AF_INET,SOCK_DGRAM, 0);
    ioctl(sockfd, SIOCGIFADDR, &ifr);
    addr = (struct sockaddr_in*)&(ifr.ifr_addr);
    ret.local_ip = addr->sin_addr.s_addr;

    ioctl(sockfd, SIOCGIFNETMASK, &ifr);
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

void find_local_hosts(struct localnet* loc_net, char*** hosts_array, int* size)
{
    uint32_t network = ntohl(loc_net->network);
    uint32_t broadcast = ntohl(loc_net->broadcast);
    uint32_t ip = network + 1;
    char host[INET_ADDRSTRLEN];
    while(ip < broadcast)
    {
       ip_to_string(htonl(ip), host, sizeof(host));
       add_host(hosts_array, size, host);
       ++ip;
    }
}
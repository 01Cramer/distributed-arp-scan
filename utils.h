struct args
{
   char* interface;
   char* target_type;
   char* target;
};

struct localnet
{
   uint32_t local_ip;
   uint32_t mask;
   uint32_t network;
   uint32_t broadcast;
};

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

struct args parse_args(int argc, char** argv);

struct localnet get_localnet_info(char* interface);

const char* ip_to_string(uint32_t ip, char* buffer, size_t size);

void find_local_hosts(struct localnet* loc_net, char*** hosts_array, int* size);

void add_host(char*** hosts_array, int* size, char* host);
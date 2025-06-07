# distributed arp-scan
# Project Description
The architecture of the solution is based on a centralized server that identifies hosts available on the local network, then distributes IP address ranges to selected processing servers (referred to as nodes). Each node is responsible for performing an ARP scan only for its assigned range and sending the results back to the central server, which aggregates the collected information.

Thanks to parallel scanning performed by multiple nodes, the time required to scan the entire network is significantly reduced, which can be especially useful in large network environments.

# Source Files and Compilation
The project consists of two source files:

centralized_server.c: contains the source code of the central server.

server_node.c: contains the source code of the node that performs scanning.

A Makefile is provided to compile both source files:

Use the command <code>make arpscan</code> to compile the central server.

Use the command <code>make node</code> to compile the node (requires prior installation of the libnet and libpcap libraries).

# How to Run
To perform a distributed arp-scan, you must first prepare the individual nodes. The compiled node program is run using the command:

<code>./node INTERFACE</code>
After starting the nodes, you can start the central server:

<code>./arpscan -i INTERFACE -l -n NODE_IP_1 -n NODE_IP_2</code>
(Multiple nodes can be specified using additional -n switches.)

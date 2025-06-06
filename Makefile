arpscan: centralized_server.c
	gcc $^ -o $@
node: server_node.c
	gcc $^ -o $@ -lnet -lpcap
.PHONY: clean
clean:
	rm arpscan node *.o
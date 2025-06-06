utils.o: utils.c
	gcc -c $^
arpscan.o: arpscan.c
	gcc -c $^
arpscan: utils.o arpscan.o
	gcc $^ -o $@ -lnet -lpcap

.PHONY: clean
clean:
	rm arpscan *.o
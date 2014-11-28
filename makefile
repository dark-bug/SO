simplehttpd: simplehttpd.c 
	gcc -lrt -pthread -Wall -o simplehttpd simplehttpd.c -I.

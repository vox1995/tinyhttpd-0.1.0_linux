httpd: httpd.c
	gcc -W -Wall -o server httpd.c -lpthread
clean:
	rm server

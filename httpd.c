/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
 
#define ISspace(x) isspace((int)(x))
 
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
 
void *accept_request(void *);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);
 
/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void *accept_request(void * tclient)
{
 int client = *(int *)tclient;
 char buf[1024];
 int numchars;
 char method[255];	 //保存请求行中的请求方法 GET or POST
 char url[255];		 //请求行的 url 字段
 char path[512];	 //请求行中文件在服务器上的路径
 size_t i, j;
 struct stat st;
 int cgi = 0;      /* becomes true if server decides this is a CGI
                    * program */
 char *query_string = NULL;	//get 请求？之后的查询参数
 
 numchars = get_line(client, buf, sizeof(buf));	//获取请求行,get_line返回值为读取到的字符个数
 i = 0; j = 0;
 /* 处理请求行 */
 /* 请求方法 */
 while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
 {
  method[i] = buf[j];	//根据 http 请求报文格式，这里得到的是请求方法
  i++; j++;
 }
 method[i] = '\0';
 /* 省略 */
 /* URL 字段 */
 //strcasecmp为忽略大小写比较两字符段，相等返回0
 if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
 {
  unimplemented(client);  //method既不是GET也不是POST，执行unimplemented，返回空
  return NULL;
 }
 
 if (strcasecmp(method, "POST") == 0)
  cgi = 1;
 //method为POST，则将cgi标识为1，准备执行cgi脚本
 
 i = 0;
 while (ISspace(buf[j]) && (j < sizeof(buf)))
  j++;
 while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
 {
  url[i] = buf[j];	//获取的是 URL（互联网标准资源的地址）
  i++; j++;
 }
 url[i] = '\0';
 
  //method为GET，准备执行GET操作
 if (strcasecmp(method, "GET") == 0)
 {
  query_string = url;	//请求信息
  while ((*query_string != '?') && (*query_string != '\0'))	//截取'?'前的字符
   query_string++;	//问号前面是路径，后面是参数
  if (*query_string == '?')	
  {
   cgi = 1;
   *query_string = '\0';
   query_string++;
  }
  //有'?'，表明动态请求
 }
 
 /* 根据 url 拼接 url 在服务器上的路径 */
 sprintf(path, "htdocs%s", url);
 if (path[strlen(path) - 1] == '/')
  strcat(path, "index.html");	//如果 url 是目录(/)，则加上 index.html
/* 查找 path 指向的文件 */
 if (stat(path, &st) == -1) {
	 /*丢弃所有 headers 的信息*/
  while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
   numchars = get_line(client, buf, sizeof(buf));	//从客户端读取数据进 buf
  not_found(client);	//回应客户端找不到
 }
 //执行失败，文件未找到
 else
 {
	  /*如果 path 是个目录，则默认使用该目录下 index.html 文件*/
  if ((st.st_mode & S_IFMT) == S_IFDIR)
   strcat(path, "/index.html");
/* 如果 path 是可执行文件，设置 cgi 标识，在浏览器输入参数时会调用 */
  if ((st.st_mode & S_IXUSR) ||
      (st.st_mode & S_IXGRP) ||
      (st.st_mode & S_IXOTH)    )
   cgi = 1;
  if (!cgi){
    printf("serve_file\n");
    serve_file(client, path);	//静态页面请求，直接返回文件信息给客户端，静态页面返回
  }
  
  else{
    printf("execute_cgi\n");
    execute_cgi(client, path, method, query_string);	//执行 cgi 脚本
  }
 }
 
 close(client);	//关闭客户端套接字
 return NULL;
}
 
/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
 char buf[1024];
 
 sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "Content-type: text/html\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "<P>Your browser sent a bad request, ");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "such as a POST without a Content-Length.\r\n");
 send(client, buf, sizeof(buf), 0);
}
 
/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
 char buf[1024];
 
 fgets(buf, sizeof(buf), resource);
 while (!feof(resource))
 {
  send(client, buf, strlen(buf), 0);
  fgets(buf, sizeof(buf), resource);
 }
}
 
/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
 char buf[1024];
 
 sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
 send(client, buf, strlen(buf), 0);
}
 
/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
 perror(sc);
 exit(1);
}
 
/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client, const char *path,
                 const char *method, const char *query_string)
{
 char buf[1024];
 int cgi_output[2];
 int cgi_input[2];
 pid_t pid; //表示fork()函数的返回值，在父进程中，fork返回新创建子进程的进程ID
 int status;
 int i;
 char c;
 int numchars = 1;
 int content_length = -1;
 
 buf[0] = 'A'; buf[1] = '\0';
 if (strcasecmp(method, "GET") == 0)	//GET 方法：一般用于获取/查询资源信息
  while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers 读取并丢弃 HTTP 请求 */
   numchars = get_line(client, buf, sizeof(buf));
 else    /* POST 一般用于更新资源信息*/
 {
  numchars = get_line(client, buf, sizeof(buf));
  //获取 HTTP 消息实体的传输长度
  while ((numchars > 0) && strcmp("\n", buf))	//不为空且不为换行符
  {
   buf[15] = '\0';
   if (strcasecmp(buf, "Content-Length:") == 0)	//是否为 Content-Length 字段
    content_length = atoi(&(buf[16]));			//Content-Length 用于描述 HTTP 消息实体的传输长度
   numchars = get_line(client, buf, sizeof(buf));
  }
  if (content_length == -1) {
   bad_request(client);							//请求的页面数据为空，没有数据，就是我们打开网页经常出现空白页面
   return;
  }
 }
 
 sprintf(buf, "HTTP/1.0 200 OK\r\n");
 send(client, buf, strlen(buf), 0);
 
 if (pipe(cgi_output) < 0) {
  cannot_execute(client);						//管道建立失败，打印出错信息
  return;
 }
 if (pipe(cgi_input) < 0) {
  cannot_execute(client);
  return;
 }
 
 if ( (pid = fork()) < 0 ) {
  cannot_execute(client);
  return;
 }
 
  //实现进程间的管道通信机制
  /*子进程继承了父进程的 pipe，然后通过关闭子进程 output 管道的输出端，input 管道的写入端；
  关闭父进程 output 管道的写入端，input 管道的输出端*/
  
 
 if (pid == 0)  /* 子进程 */
 {
  char meth_env[255];
  char query_env[255];
  char length_env[255];
 
//复制文件句柄，重定向进程的标准输入输出
//dup2 的第一个参数描述符关闭
 
  dup2(cgi_output[1], 1);		//标准输出重定向到 output 管道的写入端
  dup2(cgi_input[0], 0);		//标准输入重定向到 input 管道的读取端
  close(cgi_output[0]);			//关闭 output 管道的写入端
  close(cgi_input[1]);			//关闭输出端
  sprintf(meth_env, "REQUEST_METHOD=%s", method);
  putenv(meth_env);
  if (strcasecmp(method, "GET") == 0) {
		/*设置 query_string 的环境变量*/
   sprintf(query_env, "QUERY_STRING=%s", query_string);
   putenv(query_env);
  }
  else {   /* POST */
		/*设置 content_length 的环境变量*/
   sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
   putenv(length_env);
  }
  execl(path, path, NULL);
  /*exec数簇，用exec函数可以把当前进程替换为一个新进程，且新进程与原进程有相同的PID,
  第一个path参数表示你要启动程序的名称包括路径名,第二个参数表示启动程序所带的参数
  此处即为执行目录下的CGI脚本，获取 cgi 的标准输出作为相应内容发送给客户端*/

  exit(0);			//子进程退出
 } else {    /* 父进程 */
  close(cgi_output[1]);		//关闭管道的一端，这样可以建立父子进程间的管道通信
  close(cgi_input[0]);
  /*通过关闭对应管道的通道，然后重定向子进程的管道某端，这样就在父子进程之间构建一条单双工通道
    如果不重定向，将是一条典型的全双工管道通信机制*/
  if (strcasecmp(method, "POST") == 0)		//POST 方式，将指定好的传输长度字符发送
   /*接收 POST 过来的数据*/
   for (i = 0; i < content_length; i++) {
    recv(client, &c, 1, 0);		//从客户端接收单个字符
    write(cgi_input[1], &c, 1);	//写入 input，然后重定向到了标准输入
	  //数据传送过程：input[1](父进程) ——> input[0](子进程)[执行 cgi 函数] ——> STDIN ——> STDOUT
    // ——> output[1](子进程) ——> output[0](父进程)[将结果发送给客户端]
   }
  while (read(cgi_output[0], &c, 1) > 0)	//读取 output 的管道输出到客户端，output 输出端为 cgi 脚本执行后的内容
   send(client, &c, 1, 0);					//即将 cgi 执行结果发送给客户端，即 send 到浏览器，如果不是 POST 则只有这一处理
 
  close(cgi_output[0]);						//关闭剩下的管道端，子进程在执行 dup2 之后，就已经关闭了管道一端通道
  close(cgi_input[1]);
  waitpid(pid, &status, 0);					//等待子进程终止
 }
}
 
/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
 int i = 0;
 char c = '\0';
 int n;
  /* http 请求报文每行都是\r\n 结尾 */
 while ((i < size - 1) && (c != '\n'))
 {
  n = recv(sock, &c, 1, 0);
  /* DEBUG printf("%02X\n", c); */
  if (n > 0)
  {
   if (c == '\r')	//如果是回车符，继续读取
   {
    n = recv(sock, &c, 1, MSG_PEEK);	/* MSG_PEEK 探测下一个字符是不是\n */
    /* DEBUG printf("%02X\n", c); */
    if ((n > 0) && (c == '\n'))	//如果是回车换行符说明读完一行
     recv(sock, &c, 1, 0);
    else
     c = '\n';	//换行替换回车，当作一行返回
   }
   buf[i] = c;
   i++;
  }
  else
   c = '\n';	
 }
 buf[i] = '\0';	//返回一行
 
 return(i);	//返回读到的字符个数(包括'\0')
}
 
/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename)
{
 char buf[1024];
 (void)filename;  /* could use filename to determine file type */
 
 strcpy(buf, "HTTP/1.0 200 OK\r\n");
 send(client, buf, strlen(buf), 0);
 strcpy(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 strcpy(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
}
 
/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
 char buf[1024];
 
 sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "your request because the resource specified\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "is unavailable or nonexistent.\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</BODY></HTML>\r\n");
 send(client, buf, strlen(buf), 0);
}
 
/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename)
{
 FILE *resource = NULL;
 int numchars = 1;
 char buf[1024];
 
 buf[0] = 'A'; buf[1] = '\0';
 while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
  numchars = get_line(client, buf, sizeof(buf));
 
 resource = fopen(filename, "r");
 if (resource == NULL)
  not_found(client);
 else
 {
  headers(client, filename);
  cat(client, resource);
 }
 fclose(resource);
}
 
/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
 int httpd = 0;
 struct sockaddr_in name;
 
 httpd = socket(PF_INET, SOCK_STREAM, 0);//PF_INET表示IPv4域；SOCK_STREAM表示面向可靠的连接方式（TCP）；0指默认协议；
 if (httpd == -1)
  error_die("socket");
 memset(&name, 0, sizeof(name));//把a中所有字节换做字符“0”
 name.sin_family = AF_INET;//地址族，AF_INET指TCP/IP协议中的IPv4
 name.sin_port = htons(*port);//16位TCP端口号，htons将无符号短整型主机字节序转换为网络字节序，htons(0)系统会自动分配一个端口值；
 name.sin_addr.s_addr = htonl(INADDR_ANY);//32位IPv4地址
 if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
  error_die("bind");
//bind把一个本地协议地址赋予一个套接字，适用于未连接的数据报或流类套接口，在connect()或listen()调用前使用;
 if (*port == 0)  /* if dynamically allocating a port */
 {
  socklen_t namelen = sizeof(name);
  if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
   error_die("getsockname");
  *port = ntohs(name.sin_port);//ntohs将一个16位数由网络字节顺序转换为主机字节顺序
 }
 if (listen(httpd, 5) < 0)
  error_die("listen");
 return(httpd);
}
 
/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{
 char buf[1024];
 
 sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</TITLE></HEAD>\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</BODY></HTML>\r\n");
 send(client, buf, strlen(buf), 0);
}
 
/**********************************************************************/
 
int main(void)
{
	
 int server_sock = -1;
 u_short port = 4000;     //指定运行端口，若为0则随机生成
 int client_sock = -1;
	
 struct sockaddr_in client_name;
 //结构体定义在#include<netinet/in.h>或#include <arpa/inet.h>中，把port和addr分开储存在两个变量中，用来处理网络通信的地址；

 socklen_t client_name_len = sizeof(client_name);
//socklen_t定义在#include <sys/socket.h>中，和当前机器的int类型具有一致的字节长度，用于处理保障跨平台数据类型不统一问题；

 pthread_t newthread;//声明线程的id
 
 server_sock = startup(&port);//返回值为：socket(PF_INET, SOCK_STREAM, 0);
	//调用startup函数；
	//指定了IPV4和TCP协议，并使用bind函数对socket进行了绑定。如果端口号为0，则动态分配端口号。
	//经过startup函数后，tinyhttpd已经建立了服务端的连接，服务端进入LISTEN状态开始监听。
 printf("httpd running on port %d\n", port);
 
 while (1)
	//进入一个死循环while(1)，其中不断对server_sock也就是服务器套接字进行监听，如果存在
	//连接请求，则接受连接请求并创建客户机套接字，使用该客户机套接字建立线程，
 {

	  client_sock = accept(server_sock,
						   (struct sockaddr *)&client_name,
						   &client_name_len);
	/*accept函数定义在#include<sys/socket.h>库中，返回值是一个新的套接字描述符，它代表的是和客户端的新的连接；
	可以把它理解成是一个客户端的socket,这个socket包含的是客户端的ip和port信息 。
	这样就有两个套接字了，服务器端的在侦听创建的端口，客户端的在准备发送send()和接收recv()数据；
  accept()调用失败时返回-1*/
	
	  if (client_sock == -1)
	   error_die("accept");
	  //accept_request(client_sock); 
	  if (pthread_create(&newthread , NULL, accept_request, (void *)&client_sock) == 0)
	   perror("pthread_create");  
   
  //创建线程函数:pthread_create，返回值为0时，调用成功，输出"pthread_create"；返回值为-1时，表示出错；
	//第一个参数为指向线程标识符的指针。
	//第二个参数用来设置线程属性。
	//第三个参数是线程运行函数的起始地址。
	//最后一个参数是运行函数的参数。
  //线程执行函数:accept_request(client_sock)
 }
 
 close(server_sock);//关闭服务器套接字。
 
 return(0);

}


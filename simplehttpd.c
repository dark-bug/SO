/* 
 * -- simplehttpd.c --
 * A (very) simple HTTP server
 *
 * Sistemas Operativos 2014/2015
 */

#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/shm.h>

// Produce debug information
#define DEBUG	  	1	

// Header of HTTP reply to client 
#define	SERVER_STRING 	"Server: simpleserver/0.1.0\r\n"
#define HEADER_1	"HTTP/1.0 200 OK\r\n"
#define HEADER_2	"Content-Type: text/html\r\n\r\n"

#define GET_EXPR	"GET /"
#define CGI_EXPR	"cgi-bin/"
#define SIZE_BUF	1024
#define STRING 50
#define SCRIPTS 5
#define CONF_FILE "configurations.txt"
#define STATS_FILE "stats.txt"
#define DELIMITATOR "="
#define COMMA ","

 char buf[SIZE_BUF];
 char req_buf[SIZE_BUF];
 char buf_tmp[SIZE_BUF];
 int socket_conn,new_conn, m_queue,shmid;
 pid_t configuration;
 pid_t statistics;

 time_t timestamp(){
 	time_t ltime;
 	ltime=time(NULL);
 	return ltime; 
 }

 typedef struct shared_mem_model{
 	char port[SIZE_BUF],n[SIZE_BUF];
 	char policy[STRING];
 	char script[SCRIPTS];
 }config;

 config *conf;

/*
typedef struct statistics *stats;
 typedef struct statistics{
 	char request_type[STRING];
 	char filename[STRING];
 	char thread_number[STRING];
 	char reception_time[STRING],conclusion_time[STRING];
 	stats next;
 }stats_struct;
*/
 time_t timestamp();
 int  fireup(int port);
 void identify(int socket);
 void get_request(int socket);
 int  read_line(int socket, int n);
 void send_header(int socket);
 void send_page(int socket);
 void execute_script(int socket);
 void not_found(int socket);
 void catch_ctrlc(int);
 void cannot_execute(int socket);
 void conf_manager();
 void read_conf();
 void stats_manager();
 void catch_hangup(int sig);
/* int write_stats(stats list);
 struct statistics * load_stats();
 



stats initialize_stats_struct(void){
	stats aux;
	aux = (stats) malloc(sizeof(stats_struct));
	if(aux != NULL){
		strcpy(aux->request_type,"");
		strcpy(aux->filename,"");
		strcpy(aux->thread_number,"");
		strcpy(aux->reception_time,"");
		strcpy(aux->conclusion_time,"");
		aux->next=NULL;
	}
	return aux;
}
*/

int main(int argc, char ** argv)
{
	time_t server_init_time = timestamp();
	printf("%s",asctime(localtime(&server_init_time)));

	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);
	int port,n;

	shmid = shmget(IPC_PRIVATE, sizeof(config),IPC_CREAT|0700);
	conf = (config*)shmat(shmid,NULL,0);

 	/*stats stats_list = initialize_stats_struct();
 	stats_list = load_stats();*/

	signal(SIGINT,catch_ctrlc);
	signal(SIGHUP,catch_hangup);

 	//create configuration process
	if((configuration=fork())==0){
		conf_manager();
	}
	else if(configuration<0){
		printf("Error creating configuration process.\n");
		exit(-1);
	}
	else{
		wait(NULL);
	}

 	//create statistics process
	if((statistics=fork())==0){
		stats_manager();
	}
	else if(statistics<0){
		printf("Error creating statitstics process.\n");
		exit(-1);
	}
	else{
		wait(NULL);
	}

	port = atoi(conf->port);
	n = atoi(conf->n);

	printf("Listening for HTTP requests on port %d\n",port);
	printf("We'll be having %d threads in pool\n",n);
	printf("Scheduler follows %s",conf->policy);



	// Configure listening port
	if ((socket_conn=fireup(port))==-1)
		exit(1);

	// Serve requests 
	while (1)
	{
		// Accept connection on socket
		if ( (new_conn = accept(socket_conn,(struct sockaddr *)&client_name,&client_name_len)) == -1 ) {
			printf("Error accepting connection\n");
			exit(1);
		}

		// Identify new client
		identify(new_conn);

		// Process request
		get_request(new_conn);

		// Verify if request is for a page or script
		if(!strncmp(req_buf,CGI_EXPR,strlen(CGI_EXPR)))
			execute_script(new_conn);	
		else
			// Search file with html page and send to client
			send_page(new_conn);

		// Terminate connection with client 
		close(new_conn);

	}

}

// Processes request from client
void get_request(int socket)
{
	int i,j;
	int found_get;

	found_get=0;
	while ( read_line(socket,SIZE_BUF) > 0 ) {
		if(!strncmp(buf,GET_EXPR,strlen(GET_EXPR))) {
			// GET received, extract the requested page/script
			found_get=1;
			i=strlen(GET_EXPR);
			j=0;
			while( (buf[i]!=' ') && (buf[i]!='\0') )
				req_buf[j++]=buf[i++];
			req_buf[j]='\0';
		}
	}	

	// Currently only supports GET 
	if(!found_get) {
		printf("Request from client without a GET\n");
		exit(1);
	}
	// If no particular page is requested then we consider htdocs/index.html
	if(!strlen(req_buf))
		sprintf(req_buf,"index.html");

	#if DEBUG
	printf("get_request: client requested the following page: %s\n",req_buf);
	#endif

	return;
}


// Send message header (before html page) to client
void send_header(int socket)
{
	#if DEBUG
	printf("send_header: sending HTTP header to client\n");
	#endif
	sprintf(buf,HEADER_1);
	send(socket,buf,strlen(HEADER_1),0);
	sprintf(buf,SERVER_STRING);
	send(socket,buf,strlen(SERVER_STRING),0);
	sprintf(buf,HEADER_2);
	send(socket,buf,strlen(HEADER_2),0);

	return;
}


// Execute script in /cgi-bin
void execute_script(int socket)
{
	// Currently unsupported, return error code to client
	cannot_execute(socket);

	return;
}


// Send html page to client
void send_page(int socket)
{
	FILE * fp;

	// Searchs for page in directory htdocs
	sprintf(buf_tmp,"htdocs/%s",req_buf);

	#if DEBUG
	printf("send_page: searching for %s\n",buf_tmp);
	#endif

	// Verifies if file exists
	if((fp=fopen(buf_tmp,"rt"))==NULL) {
		// Page not found, send error to client
		printf("send_page: page %s not found, alerting client\n",buf_tmp);
		not_found(socket);
	}
	else {
		// Page found, send to client 

		// First send HTTP header back to client
		send_header(socket);

		printf("send_page: sending page %s to client\n",buf_tmp);
		while(fgets(buf_tmp,SIZE_BUF,fp))
			send(socket,buf_tmp,strlen(buf_tmp),0);

		// Close file
		fclose(fp);
	}

	return; 

}


// Identifies client (address and port) from socket
void identify(int socket)
{
	char ipstr[INET6_ADDRSTRLEN];
	socklen_t len;
	struct sockaddr_in *s;
	int port;
	struct sockaddr_storage addr;

	len = sizeof addr;
	getpeername(socket, (struct sockaddr*)&addr, &len);

	// Assuming only IPv4
	s = (struct sockaddr_in *)&addr;
	port = ntohs(s->sin_port);
	inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

	printf("identify: received new request from %s port %d\n",ipstr,port);

	return;
}


// Reads a line (of at most 'n' bytes) from socket
int read_line(int socket,int n) 
{ 
	int n_read;
	int not_eol; 
	int ret;
	char new_char;

	n_read=0;
	not_eol=1;

	while (n_read<n && not_eol) {
		ret = read(socket,&new_char,sizeof(char));
		if (ret == -1) {
			printf("Error reading from socket (read_line)");
			return -1;
		}
		else if (ret == 0) {
			return 0;
		}
		else if (new_char=='\r') {
			not_eol = 0;
			// consumes next byte on buffer (LF)
			read(socket,&new_char,sizeof(char));
			continue;
		}		
		else {
			buf[n_read]=new_char;
			n_read++;
		}
	}

	buf[n_read]='\0';
	#if DEBUG
	printf("read_line: new line read from client socket: %s\n",buf);
	#endif

	return n_read;
}


// Creates, prepares and returns new socket
int fireup(int port)
{
	int new_sock;
	struct sockaddr_in name;

	// Creates socket
	if ((new_sock = socket(PF_INET, SOCK_STREAM, 0))==-1) {
		printf("Error creating socket\n");
		return -1;
	}

	// Binds new socket to listening port 
	name.sin_family = AF_INET;
	name.sin_port = htons(port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(new_sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
		printf("Error binding to socket\n");
		return -1;
	}

	// Starts listening on socket
	if (listen(new_sock, 5) < 0) {
		printf("Error listening to socket\n");
		return -1;
	}

	return(new_sock);
}


// Sends a 404 not found status message to client (page not found)
void not_found(int socket)
{
	sprintf(buf,"HTTP/1.0 404 NOT FOUND\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,SERVER_STRING);
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"Content-Type: text/html\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<HTML><TITLE>Not Found</TITLE>\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<BODY><P>Resource unavailable or nonexistent.\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"</BODY></HTML>\r\n");
	send(socket,buf, strlen(buf), 0);

	return;
}


// Send a 5000 internal server error (script not configured for execution)
void cannot_execute(int socket)
{
	sprintf(buf,"HTTP/1.0 500 Internal Server Error\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"Content-type: text/html\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<P>Error prohibited CGI execution.\r\n");
	send(socket,buf, strlen(buf), 0);

	return;
}


// Closes socket before closing
void catch_ctrlc(int sig)
{
	printf("Server terminating\n");
	close(socket_conn);
	shmdt(conf);
	shmctl(shmid,IPC_RMID,NULL);
	exit(0);
}

void catch_hangup(int sig){
	printf("Catched SIGHUP.\n");
	read_conf();
}

void conf_manager(){

	read_conf();
	
 	//add to sleep
}


void read_conf(){
	FILE *f = fopen(CONF_FILE,"r");


	if(f)
		printf("Path is valid\n");
	else{
		printf("\nPath is not valid\n");
		system("pause");
		exit(0);
	}

	if(f!=NULL){
		int i=1;
		char line[SIZE_BUF];
		printf("Reading configurations.txt file\n\n");

		while(fgets(line,sizeof(line),f)!=NULL){
			char *temp;
			temp = strstr((char *)line,DELIMITATOR);
			temp = temp + strlen(DELIMITATOR);

			switch(i){
				case 1:
				memcpy(conf->port,temp,strlen(temp));
				break;
				case 2:
				memcpy(conf->n,temp,strlen(temp));
				break;
				case 3:
				memcpy(conf->policy,temp,strlen(temp));
				break;
				case 4:
				memcpy(conf->script,temp,strlen(temp));
				break;
			}
			i++;
		}
	}
	else{
		printf("No configurations file found\n");
		f=fopen(CONF_FILE,"w");
	}
	fclose(f);

}

void stats_manager(){


}
/*
int write_stats(stats list){
 	FILE *f = fopen(STATS_FILE,"w");
 	list=list->next;

 	if(f){
 		printf("Writing statistics to file\n");
 		while(list!=NULL){
 			fprintf(f,"%s,%s,%s,%s,%s;\n",list->request_type,list->filename,list->thread_number,asctime(localtime(&list->reception_time)),asctime(localtime(&list->conclusion_time)));
 			list=list->next;
 		}
 	}
 	else{
 		printf("Something went wrong, can't open the stats file!\n");
 		exit(1);
 	}
 	fclose(f);
 	return 1;
 }

struct statistics * load_stats(){
 	FILE *f = fopen(STATS_FILE,"r");
 	struct statistics * aux;

 	
 	if(f){
 		printf("Loading statistics from file.\n");

 		char line[SIZE_BUF*2],*temp;
 		int i=1;
 		while(fgets(line,sizeof(line),f)!=NULL){
 			temp = strstr((char *)line,DELIMITATOR);
 			temp = temp + strlen(DELIMITATOR);

 			switch(i){
 				case 1: 
 				memcpy(aux->request_type,temp,strlen(temp));
 				break;
 				case 2:
 				memcpy(aux->filename,temp,strlen(temp));
 				break;
 				case 3:
 				memcpy(aux->thread_number,temp,strlen(temp));
 				break;
 				case 4:
 				memcpy(aux->reception_time,temp,strlen(temp));
 				break;
 				case 5:
 				memcpy(aux->conclusion_time,temp,strlen(temp));
 				break;
 			}
 			i++;
 		}
 	

 	}
 	fclose(f);
 	
 }
*/



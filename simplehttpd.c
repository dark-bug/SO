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
#include <assert.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>

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
#define PEDIDO_ESTATICO 0
#define PEDIDO_DINAMICO 1
#define PIPE_NAME "one_pipe_to_rule_them_all"
#define FIFO 0
#define PRIORIDADE_ESTATICO 1
#define PRIORIDADE_DINAMICO 2

 char buf[SIZE_BUF];
 char req_buf[SIZE_BUF];
 char buf_tmp[SIZE_BUF];
 int socket_conn,new_conn, m_queue,shmid,policy;
 pid_t configuration;
 pid_t childs[2];
 time_t server_init_time;
 int conta_pedidos, fd[2];


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


 typedef struct statistics{
 	long mtype;
 	char request_type[STRING];
 	char filename[STRING];
 	int thread_number;
 	char reception_time[STRING],conclusion_time[STRING];
 }stats;

 typedef struct pedido
 {
 	int socket;
 	char ficheiro[SIZE_BUF];
 	int tipo_pedido;
 	int n_pedido;
 	int id_thread;
 	char reception_time[STRING];
 } PEDIDO;
 
 typedef struct node_type
 {
 	PEDIDO pedido;
 	struct node_type *next;
 }NODE_TYPE;

 typedef NODE_TYPE *NODE_PTR;

 typedef struct
 {
 	NODE_TYPE *rear;
 	NODE_TYPE *front;
 }Q_TYPE;

 typedef struct buffer
 {
 	Q_TYPE queue_principal;
 	Q_TYPE queue_prioridade;
 	PEDIDO prox_ped_atender;
 }BUFFER;

 BUFFER buff;

 sem_t *full;
 sem_t *full2;
 sem_t *empty;
 sem_t *mutex;
 sem_t *mutex2;

 time_t timestamp();
 int  fireup(int port);
 void identify(int socket);
 void get_request(int socket);
 int  read_line(int socket, int n);
 void send_header(int socket);
 void send_page(int socket, char ficheiro[SIZE_BUF]);
 void execute_script(int socket);
 void not_found(int socket);
 void catch_ctrlc(int);
 void cannot_execute(int socket);
 void conf_manager();
 void read_conf();
 void stats_manager();
 void request_to_queue(stats aux);
 void catch_hangup(int sig);
 void cleanup();
 void write_stats();
 void display_stats();
 void *sched();
 void *workers(void* id);
 void init();
 void define_policy(char policy[STRING]);
 void create_queue(Q_TYPE *queue);
 int empty_queue(Q_TYPE *queue);
 void destroy_queue(Q_TYPE *queue);
 void enqueue(Q_TYPE *queue,int sckt,char fchr[SIZE_BUF], int tp );
 PEDIDO dequeue(Q_TYPE *queue);
 void trata_pedido(int socket,char ficheiro[SIZE_BUF] , int tipo_pedido );
 int check_script_request(char request[STRING]);
 stats prepare_stats();
 int trata_dinamico(PEDIDO req);


 int main(int argc, char ** argv)
 {
 	int i;

 	init();
 	
 	pthread_t SCHEDULER;
 	pthread_t POOL[atoi(conf->n)];

 	server_init_time = timestamp();
 	printf("%s",asctime(localtime(&server_init_time)));

 	struct sockaddr_in client_name;
 	socklen_t client_name_len = sizeof(client_name);
 	int port,n;

 	signal(SIGINT,catch_ctrlc);
 	signal(SIGHUP,catch_hangup);


 	port = atoi(conf->port);
 	n = atoi(conf->n);
 	//int id [atoi(conf->n)];

 	int id[atoi(conf->n)];
 	pthread_create(&SCHEDULER,NULL,sched,NULL);
 	for(i = 0;i<atoi(conf->n);i++)
 	{
 		id[i] = i;
 		pthread_create(&POOL[i],NULL,workers,&id[i]);
 	}
 	printf("Listening for HTTP requests on port %d\n",port);
 	printf("We'll be having %d threads in pool\n",n);
 	printf("Scheduler follows %s",conf->policy);

 	if((childs[0]=fork())==0){
 		printf("Initializing statistics manager\n");
 		stats_manager();
 		exit(0);
 	}

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
 		//if(!strncmp(req_buf,CGI_EXPR,strlen(CGI_EXPR))){
 		int res = check_script_request(req_buf);
 		if(res==1){
 				trata_pedido(new_conn,req_buf,PEDIDO_DINAMICO);

 			}
 			
 		
 		else
			// Search file with html page and send to client
 			//send_page(new_conn);
 		{
 			trata_pedido(new_conn,req_buf,PEDIDO_ESTATICO);
 		}

		// Terminate connection with client 
 		//close(new_conn);

 	}

 	pthread_join(SCHEDULER,NULL);
 	for(i = 0;i<atoi(conf->n);i++)
 	{
 		pthread_join(POOL[i],NULL);
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
 void send_page(int socket,char ficheiro[SIZE_BUF])
 {
 	FILE * fp;

	// Searchs for page in directory htdocs
 	sprintf(buf_tmp,"htdocs/%s",ficheiro);

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
 	//display_stats();
 	cleanup();
 	exit(0);
 }

 void catch_hangup(int sig){
 	printf("Catched SIGHUP.\n");
 	read_conf();
 }

 void conf_manager(){

 	read_conf();
 	define_policy(conf->policy);
 	//add to sleep
 	
 }
 
 void read_conf(){

 	FILE *f = fopen(CONF_FILE,"r");

 	if(f)
 		printf("\nPath is valid.\n");
 	else{
 		printf("\nPath is not valid\n");
 		system("pause");
 		exit(0);
 	}

 	if(f!=NULL){
 		int i=1;
 		char line[SIZE_BUF];


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
 		printf("Read configurations.txt file.\n\n");
 	}
 	else{
 		printf("No configurations file found\n");
 		f=fopen(CONF_FILE,"w");
 	}
 	fclose(f);

 }

 void stats_manager(){

 	//signal(SIGINT,display_stats);
 	write_stats();
 }

 stats prepare_stats(){
 	stats aux;
 	aux.mtype=1;
 	if(buff.prox_ped_atender.tipo_pedido == PEDIDO_ESTATICO)
 		strcpy(aux.request_type,"ESTATICO");
 	strcpy(aux.filename,buff.prox_ped_atender.ficheiro);
 	strcpy(aux.reception_time,buff.prox_ped_atender.reception_time);
 	aux.thread_number = buff.prox_ped_atender.id_thread;
 	time_t current_time = timestamp();
 	strcpy(aux.conclusion_time,asctime(localtime(&current_time)));
 	return aux;
 }


 void write_stats(){
 	FILE *f = fopen(STATS_FILE,"a");
 	stats aux;

 	if(f){
 		printf("Path to file is valid.\n");
 	}
 	else{
 		printf("Something went wrong, can't write the stats file!\n");
 		exit(1);
 	}
 	
 	while(1){
 		msgrcv(m_queue,&aux,sizeof(aux),0,0);
 		printf("Received message.\nWriting to file.\n");
 		strtok(aux.reception_time, "\n");
 		printf("%s,%s,%d,%s,%s\n",aux.request_type,aux.filename,aux.thread_number,aux.reception_time,aux.conclusion_time);
 		fprintf(f,"%s,%s,%d,%s,%s \n",aux.request_type,aux.filename,aux.thread_number,aux.reception_time,aux.conclusion_time);
 		printf("Written to file.\n");
 	}

 	fclose(f);
 }

 void request_to_queue(stats aux){

 	if(msgsnd(m_queue,&aux,sizeof(aux)-sizeof(long),0)!=-1)
 		printf("Sent statistics to message queue.\n");
 	else{
 		printf("Error sending statistics to message queue.\n");
 		system("pause");
 		exit(0);
 	}
 }


 void display_stats(){
 	time_t current_time = timestamp();
 	int n_static=14,n_dynamic=13,n_refused=2;

 	printf("Server initiated: %s | Current time: %s",asctime(localtime(&server_init_time)),asctime(localtime(&current_time)));
 //esta a devolver a data mal, ver para nao ter este bug
 	printf("Number of successful requests to static content: %d", n_static);
 	printf("Number of successful requests to dynamic content: %d", n_dynamic);
 	printf("Number of refused requests: %d", n_refused);

 }


 void cleanup(){
 	//cleaning shared memory
 	shmdt(conf);
 	shmctl(shmid,IPC_RMID,NULL);
 	//removing the queue
 	msgctl(m_queue,IPC_RMID,0);
 	destroy_queue(&buff.queue_prioridade);
 	destroy_queue(&buff.queue_principal);
 	sem_close(empty);
 	sem_close(full);
 	sem_close(mutex);
 }

 int check_script_request(char request[STRING]){

 	char *token;
 	token = strtok(conf->script,",");
 	while(token != NULL){
 		printf("%s\n",token);
 		if(strcmp(token,request)==0)
 			return 1;
 		token = strtok(NULL,",");
 	}
 	return 0;
 }


 void init(){

 	create_queue(&(buff.queue_principal));
 	create_queue(&(buff.queue_prioridade));

 	conta_pedidos=0;

 	shmid = shmget(IPC_PRIVATE, sizeof(config),IPC_CREAT|0700);
 	conf = (config*)shmat(shmid,NULL,0);

 	if((m_queue = msgget(IPC_PRIVATE,IPC_CREAT|0700)) < 0){
 		printf("Error initializing message queue.\n");
 	}
 	else{
 		printf("Created message queue.\n");
 	}

//create configuration process
 	if((configuration=fork())==0){
 		conf_manager();
 		exit(0);
 	}
 	else if(configuration<0){
 		printf("Error creating configuration process.\n");
 		exit(-1);
 	}
 	else{
 		wait(NULL);
 	}

 	//create statistics process
 	

 	sem_unlink("EMPTY");
 	empty = sem_open("EMPTY",O_CREAT|O_EXCL,0700,atoi(conf->n)*2);
 	sem_unlink("FULL");
 	full = sem_open("FULL",O_CREAT|O_EXCL,0700,0);
 	sem_unlink("FULL2");
 	full2 = sem_open("FULL2",O_CREAT|O_EXCL,0700,0);
 	sem_unlink("MUTEX");
 	mutex = sem_open("MUTEX",O_CREAT|O_EXCL,0700,1);
 	sem_unlink("MUTEX2");
 	mutex2 = sem_open("MUTEX2",O_CREAT|O_EXCL,0700,1);
 }
 
 void define_policy(char input[STRING]){
 	//printf("PP = |%s|",input);
 	if(strcmp(input,"FIFO\n")==0)
 		policy = FIFO;
 	else if(strcmp(input,"STATIC\n")==0)
 		policy = PRIORIDADE_ESTATICO;
 	else if(strcmp(input,"DYNAMIC\n")==0)
 		policy = PRIORIDADE_DINAMICO;
 }

 void create_queue(Q_TYPE *queue)
 {
 	queue->front = NULL;
 	queue->rear = NULL;
 }

 int empty_queue(Q_TYPE *queue)
 {
 	return(queue->front == NULL ? 1 : 0);
 }

 void destroy_queue(Q_TYPE *queue)
 {
 	NODE_PTR temp_ptr;
 	while(empty_queue(queue)==0)
 	{
 		temp_ptr = queue->front;
 		queue->front = queue->front->next;
 		free(temp_ptr);
 	}
 	queue->rear = NULL;
 }

 void enqueue(Q_TYPE *queue,int sckt,char fchr[SIZE_BUF], int tp )
 {
 	NODE_PTR temp_ptr;
 	PEDIDO pedido_temp;
 	temp_ptr = (NODE_PTR) malloc (sizeof(NODE_TYPE));
 	pedido_temp.tipo_pedido  = tp;
 	pedido_temp.socket = sckt;
 	pedido_temp.n_pedido = conta_pedidos;

 	time_t current_time = timestamp();
 	strcpy(pedido_temp.reception_time,asctime(localtime(&current_time)));

 	conta_pedidos++;
 	strcpy(pedido_temp.ficheiro,fchr);
 	if(temp_ptr != NULL)
 	{
 		temp_ptr->pedido = pedido_temp;
 		temp_ptr->next = NULL;
 		if(empty_queue(queue)==1)
 		{
 			queue->front = temp_ptr;
 		}
 		else queue->rear->next = temp_ptr;
 		queue->rear = temp_ptr;
 	}
 }

 PEDIDO dequeue(Q_TYPE *queue)
 {
 	NODE_PTR temp_ptr;
 	PEDIDO pd;
 	if(empty_queue(queue) == 0)
 	{
 		temp_ptr = queue->front;
 		pd = temp_ptr->pedido;
 		queue->front = queue->front->next;
 		if(empty_queue(queue) == 1)
 			queue->rear = NULL;
 		free(temp_ptr);	
 	}
 	return(pd);
 } 



 void *workers(void *id)
 {
 	while(1){

 		sem_wait(full2);
 		buff.prox_ped_atender.id_thread = *((int*)id);
 		if(strcmp(buff.prox_ped_atender.ficheiro,"favicon.ico")!=0){
 			if((buff.prox_ped_atender.tipo_pedido) == PEDIDO_ESTATICO){
 				send_page(buff.prox_ped_atender.socket,buff.prox_ped_atender.ficheiro);
 			}
 			else{
 				trata_dinamico(buff.prox_ped_atender);
 			}
 			stats temp = prepare_stats();
 			request_to_queue(temp);
 			printf("\nPedido %d,atendido!\n",buff.prox_ped_atender.n_pedido);
 			close(buff.prox_ped_atender.socket);
 			
 		}
 	}
 }

 void *sched()
 {
 	PEDIDO pedido_temp;	
 	while(1){
 		if(policy == FIFO)
 		{
 			sem_wait(full);
 			sem_wait(mutex);
 			pedido_temp = dequeue(&buff.queue_principal);
 			buff.prox_ped_atender = pedido_temp;
 			sem_post(full2);
 			sem_post(mutex);
 			sem_post(empty);
 		}
 		else
 		{
 			sem_wait(full);
 			sem_wait(mutex);
 			if(empty_queue(&buff.queue_prioridade)==0)
 			{
 				pedido_temp = dequeue(&buff.queue_prioridade);
 				buff.prox_ped_atender = pedido_temp;
 			}
 			else
 			{
 				pedido_temp = dequeue(&buff.queue_principal);
 				buff.prox_ped_atender = pedido_temp;	
 			}
 			sem_post(full2);
 			sem_post(mutex);
 			sem_post(empty);
 		}
 	}
 }
 

 void trata_pedido(int skt,char fich[SIZE_BUF] , int tipo_ )
 {
 	printf("\npolicy: %d\n",policy);
 	int valor_sem;
 	sem_getvalue(full,&valor_sem);
 	printf("\n\nvalor sem: %d\n\n ", valor_sem);
 	if(valor_sem!=atoi(conf->n)*2){
 		if(policy==FIFO)
 		{
 			sem_wait(empty);
 			sem_wait(mutex);
 			printf("\nPedido Colocado na fila principal...\n");
 			enqueue(&buff.queue_principal,skt,fich,tipo_);
 			sem_post(mutex);
 			sem_post(full);
 		}
 		else if(policy == PRIORIDADE_ESTATICO)
 		{
 			if(tipo_ == PEDIDO_ESTATICO)
 			{
 				sem_wait(empty);
 				sem_wait(mutex);
 				printf("\nPedido estatico colocado na fila prioritaria!\n");
 				enqueue(&buff.queue_prioridade,skt,fich,tipo_);
 				sem_post(mutex);
 				sem_post(full);
 			}
 			else
 			{
 				sem_wait(empty);
 				sem_wait(mutex);
 				printf("\nPedido dinamico colocado na fila normal!\n");
 				enqueue(&buff.queue_principal,skt,fich,tipo_);
 				sem_post(mutex);
 				sem_post(full);	
 			}
 		}
 		else if(policy == PRIORIDADE_DINAMICO)
 		{
 			if(tipo_ == PEDIDO_DINAMICO)
 			{
 				sem_wait(empty);
 				sem_wait(mutex);
 				printf("\nPedido dinamico colocado na fila prioritaria!\n");
 				enqueue(&buff.queue_prioridade,skt,fich,tipo_);
 				sem_post(mutex);
 				sem_post(full);
 			}
 			else
 			{
 				sem_wait(empty);
 				sem_wait(mutex);
 				printf("\nPedido estatico colocado na fila normal!\n");
 				enqueue(&buff.queue_prioridade,skt,fich,tipo_);
 				sem_post(mutex);
 				sem_post(full);
 			}
 		}
 	}
 	else
 	{
 		send_page(skt,"indisponivel.html");
 		close(skt);
 	}
 }

 int trata_dinamico(PEDIDO req){
 	int fd[2],n=0;
 	char aux[STRING];
 	char aux_buff[SIZE_BUF];
 	pid_t proc;
 	sprintf(aux,"scripts/%s",req.ficheiro);
 	printf("%s",aux);
 	if(pipe(fd)){
 		printf("Can't create pipe.\n");
 		cannot_execute(req.socket);
 		return 0;
 	}
 	if((proc=fork())==0){
 		dup2(fd[1],fileno(stdout));
 		close(fd[0]);
 		close(fd[1]);
 	
 		execlp(aux,aux,NULL);
 	}
 	else{
 		close(fd[1]);
 		do{
 			n = read(fd[0],aux_buff,sizeof(aux_buff));
 			if(n>0){
 				aux_buff[n] = '\0';
 				printf("%s",aux_buff);
 				send(req.socket,aux_buff,strlen(aux_buff),0);
 			}
 		}while(n>0);
 	}
 	waitpid(proc,NULL,0);
 	return 1;
 }










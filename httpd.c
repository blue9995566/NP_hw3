/*
** httpd.c -- the np3 part two -httpd server.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h> //file control

// variable define
#define PORT "3490"  // the port users will be connecting to
#define BACKLOG 10	 // how many pending connections queue will hold
#define BYTES 1024

// function forward declaration
void parse_http_request(char * request);

//global variable declaration
static char *filename;

void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		//it prevnet the error message "Address already in use."
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}

	freeaddrinfo(servinfo);  // all done with this structure

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		int fd;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}
		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		char http_request[99999];
		bzero(http_request,99999);
		if((recv(new_fd,http_request,99999,0))==-1){
			perror("recv error");
			return 1;
		}
		printf("***%d\n%s\n***\n",(int)strlen(http_request),http_request);
		if((int)strlen(http_request)>0) parse_http_request(http_request); //may recevie nothing.


		char *querystring=getenv("QUERY_STRING");
		char path[50]="/home/ekko";  //Need to set Root diretory.
		strcat(path,filename);
		printf("------");
		if(querystring!=NULL)printf("filename:%s\nquerystring:%s\n",filename,querystring);
		printf("------");

		//after parse the request.
		//1.Decide the open file type.
		//2.Before fork(),set the status of http message.

		//may need to test .cgixxx
		
		int filename_len=(int)strlen(filename);
		// printf("str_size:%d\n",filename_len);
		// printf("%c\n",filename[filename_len-4]);
		// printf("%c\n",filename[filename_len-3]);
		// printf("%c\n",filename[filename_len-2]);
		// printf("%c\n",filename[filename_len-1]);
	
		// if(strstr(filename,".cgi\0")!=NULL){  //execute cgi program.
		if((int)strlen(filename)>=5 && filename[filename_len-4]=='.' && filename[filename_len-3]=='c' && filename[filename_len-2]=='g' && filename[filename_len-1]=='i' ){  //execute cgi program.
			// printf("filename:%s\n",filename);
			// printf("execute cgi program.\n");
			write(new_fd, "HTTP/1.1 200 OK\n", 16);
			if(!fork()){
				dup2(new_fd,1);
				// printf("HTTP/1.0 200 OK\n\n");
				char path[50]="/home/ekko";  //Need to set Root diretory.
				strcat(path,filename);
				char * args[2]={path,NULL};
				if (execvp(args[0],args) == -1){
					printf("The file doesn't exist :%s\n", path);
					_exit(EXIT_FAILURE); // If child fails
				}
			}
			
		}
		else{
			printf("open a file.\n");
			if (!fork()) { // this is the child process
				close(sockfd); // child doesn't need the listener
				// char *path="/home/ekko/index.html";
				printf("file path:%s\n",path);
				int bytes_read;
				char data_to_send[1024];
				if ( (fd=open(path, O_RDONLY))!=-1 )    //FILE FOUND
				{
					write(new_fd, "HTTP/1.1 200 OK\n\n", 17);
					// send(new_fd, "HTTP/1.0 200 OK\n\n", 17, 0);
					while ( (bytes_read=read(fd, data_to_send, BYTES))>0 )
						write (new_fd, data_to_send, bytes_read);
				}
				else    write(new_fd, "HTTP/1.1 404 Not Found\n\n", 23); //FILE NOT FOUND

				close(fd);
				close(new_fd);
				exit(0);
			}
		}
		close(fd);
		close(new_fd);  // parent doesn't need this.
	}

	return 0;
}


//Parse the request and set the "QUERY_STRING" environment variables.
void parse_http_request(char * request){
	char *tok;
	char *mark;
	char *query;
	tok=strtok(request," \t");
	while(strstr(tok,"HTTP")==NULL){
		// printf("%s\n",tok);
		if(strstr(tok,"?")!=NULL){
			mark=strchr(tok,'?');
			filename=strtok(tok,"?");
			query=mark+1;
			break;
		}
		filename=tok;
		query=NULL;
		tok=strtok(NULL," \t");
	}
	if(strncmp(filename,"/\0",2)==0) filename="/index.html";
	if(query!=NULL) setenv("QUERY_STRING",query,1);
	else unsetenv("QUERY_STRING");
}
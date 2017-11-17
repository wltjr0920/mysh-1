#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include "commands.h"
#include "built_in.h"

#define SOCK_PATH "tpf_unix_sock.socket"
#define CLIENT_PATH "tpf_unix_sock.client"

static struct built_in_command built_in_commands[] = {
  { "cd", do_cd, validate_cd_argv },
  { "pwd", do_pwd, validate_pwd_argv },
  { "fg", do_fg, validate_fg_argv }
};
char pathresol[5][256]={"/usr/local/bin","/usr/bin","/bin","/usr/sbin","/sbin"};
int status=0;
static int is_built_in_command(const char* command_name)
{
  static const int n_built_in_commands = sizeof(built_in_commands) / sizeof(built_in_commands[0]);

  for (int i = 0; i < n_built_in_commands; ++i) {
    if (strcmp(command_name, built_in_commands[i].command_name) == 0) {
      return i;
    }
  }

  return -1; // Not found
}
int evaluate_command(int n_commands, struct single_command (*commands)[512]);
void client(struct single_command (*commands)[512]);
void *server(void *arg) {

	int server_socket,client_socket,rc;
    int backlog = 10;
	struct sockaddr_un server_addr,client_addr;
	

	server_socket=socket(AF_UNIX,SOCK_STREAM,0);
	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sun_family=AF_UNIX;
	strcpy(server_addr.sun_path,SOCK_PATH);

	unlink(SOCK_PATH);
	while(1){
		if(bind(server_socket,(struct sockaddr*)&server_addr,sizeof(server_addr))==-1){
		exit(0);
	}
		rc = listen(server_socket, backlog);
   		 if (rc == -1){ 
        close(server_socket);
        exit(1);
    }
		if(listen(server_socket,5)==-1){
			exit(0);
		}
	
		int client_addr_size=sizeof(client_addr);
		client_socket=accept(server_socket,(struct sockaddr*)&client_addr,&client_addr_size);
		if(fork()==0)
		{
			close(0);
			dup2(client_socket,0);
			struct single_command proc[512];
			struct single_command *b=(struct single_command *)arg;
			memcpy(&proc[0],b,sizeof(struct single_command));
			evaluate_command(1,&proc);
			exit(0);
		}
		wait(&status);
		close(client_socket);
		close(server_socket);
		pthread_exit(0);
		exit(0);
	}
}
void client(struct single_command (*commands)[512]){
		int client_socket,rc,len;
		struct sockaddr_un server_sockaddr, client_sockaddr;
		struct single_command a[512];
		struct single_command b[512];
		struct single_command *c1=(*commands);
		struct single_command *c2=&(*commands)[1];
		
		memcpy(&a[0],c1,sizeof(struct single_command));
		memcpy(&b[0],c2,sizeof(struct single_command));

		pthread_t thread_id;
		if(pthread_create(&thread_id,NULL,server,&b)<0){
		perror(" create error : ");
		exit(0);
		}
	
		if(!fork())
		{
			client_socket=socket(AF_UNIX,SOCK_STREAM,0);
			if (client_socket == -1) {
			exit(1);
			}
			client_sockaddr.sun_family = AF_UNIX;
			len = sizeof(client_sockaddr);
    
			unlink(CLIENT_PATH);
			rc = bind(client_socket, (struct sockaddr *) &client_sockaddr, len);
			if (rc == -1){
				close(client_socket);
				exit(1);
			}
			memset(&server_sockaddr,0,sizeof(server_sockaddr));
			server_sockaddr.sun_family=AF_UNIX;
			strcpy(server_sockaddr.sun_path,SOCK_PATH);
			while(connect(client_socket,(struct sockaddr*)&server_sockaddr,sizeof(server_sockaddr))==-1);
			if(!fork())
			{
				
				close(0);close(1);
				dup2(client_socket,1);
				evaluate_command(1,&a);
				pthread_exit(0);exit(0);
			}
			wait(&status);
			pthread_join(thread_id,NULL);
			close(client_socket);
			exit(0);
		}
wait(&status);
}
int evaluate_command(int n_commands, struct single_command (*commands)[512])
{
   if(n_commands>1){
	   client(commands);
	}
    else if (n_commands ==1) {
	struct single_command* com = (*commands);

	assert(com->argc != 0);

	int built_in_pos = is_built_in_command(com->argv[0]);
	if (built_in_pos != -1) {
	    if (built_in_commands[built_in_pos].command_validate(com->argc, com->argv)) {
			if (built_in_commands[built_in_pos].command_do(com->argc, com->argv) != 0) {
				fprintf(stderr, "%s: Error occurs\n", com->argv[0]);
			}
			} else {
			fprintf(stderr, "%s: Invalid arguments\n", com->argv[0]);
			return -1;
			}
		} else if (strcmp(com->argv[0], "") == 0) {
			return 0;
		} else if (strcmp(com->argv[0], "exit") == 0) {
	    return 1;
		} else if (com->argc>0){
			pid_t pid=fork();
			if(pid==0){
				execv(com->argv[0],com->argv);
				for(int i=0;i<5;i++){
					char *temp;
					temp=strcat(pathresol[i],"/");
					temp=strcat(temp,com->argv[0]);
					execv(temp,com->argv);
				}
				exit(1);
			}else{
				waitpid(pid,0,0);
			}
		}
			else {
			fprintf(stderr, "%s: command not found\n", com->argv[0]);
			return -1;
		}
    }

    return 0;
}

void free_commands(int n_commands, struct single_command (*commands)[512])
{
    for (int i = 0; i < n_commands; ++i) {
	struct single_command *com = (*commands) + i;
	int argc = com->argc;
	char** argv = com->argv;

	for (int j = 0; j < argc; ++j) {
	    free(argv[j]);
	}

	free(argv);
    }

    memset((*commands), 0, sizeof(struct single_command) * n_commands);
}
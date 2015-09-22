/*filename:remoteServer.c*/
#include <sys/types.h>
#include <sys/socket.h>      //socket system calls
#include <netinet/in.h>      //for internet sockets
#include <sys/select.h>      //for select system call
#include <stdio.h>           //printfs
#include <unistd.h>
#include <netdb.h>           //for hton etc...
#include <stdlib.h>          //malloc
#include <string.h>
#include <errno.h>           //error numbering
#include <time.h>
#include <signal.h>          //signal manipulation
#define READ 0
#define WRITE 1
#define BUFSIZE 512

int fd[2];
//inputs
int noc,port,probfail;      //noc=number of children to be born
char* gbl_port;
struct hostent *rem;
//processes
pid_t pid,parent_id,*pid_array;
//reuse ip addresses
int socket_option_value=1;
unsigned int sock,newsock;

sig_atomic_t child_exit_status;

void time_to_stop(int signo){
	int i;
	if(getpid()==parent_id){
		for(i=0;i<noc;i++)
			if(kill(pid_array[i],SIGUSR1)<0){
				perror("Couldn't perform kill from pid_array in time_to_stop");
				exit(1);
			}
		kill(parent_id,SIGTERM);
		close(sock);
		close(newsock);
	}
		
}

void clean_up_child(int signo) 
{ 
	//Clean up the child process
	int status; 
	if(wait(&status)!=pid){
		perror("wait");
		exit(1);
	}
	close(fd[READ]);
	close(sock);
	close(newsock);
	fprintf(stderr,"Child process with PID:%d was terminated\n EXIT STATUS:%d\n",pid,status >> 8);
	//Store its exit status in a global variable 
	child_exit_status = status; 
} 

int parse(int file_d,struct hostent* rem){
	int i=0,start,bytesread; 
	char command[BUFSIZE],dgrport[BUFSIZE];
	
	
	close(fd[READ]);
	
	if((bytesread=read(file_d,dgrport,strlen(dgrport)+1))<0){
		perror("Couldn't read datagram port from client");
		return 2;
	}
	
	if(write(file_d,"Enter a new command:",25)<0){
		perror("write of new command request");
		return 1;
	}

	if((bytesread=read(file_d,command,sizeof(command)))<0){
		perror("read from parse's client read");
		return 1;
	}

	//no input from client even after timeval period?? or unavailable source or signal???
	if(bytesread==0)
		return 0;

	for(i=0;i<512;i++){
		//case of large command
		if(i==255 && command[i]!='\n')
			return -1;
	}
	
	//FIRST WRITE
	//write datagram port buffer...it's really the global used port but
	//this is a more elegant way to pass it to the hild process
	if(write(fd[WRITE],dgrport,sizeof(dgrport)+1)<0){
		perror("write in parse of datagram's port containing buffer");
		exit(1);
	}
	
	printf("I just wrote to pipe's edge %d the struct for tha host's name",fd[WRITE]);
	//SECOND WRITE
	//write command buffer
	if(write(fd[WRITE],command,sizeof(command))<0){
		perror("write in parse of command containing buffer");
		exit(1);
	}
	printf("I just wrote to pipe's edge %d the command read the client %s",fd[WRITE],command);
	
	//THIRD WRITE
	//hostname
	if(write(fd[WRITE],rem->h_name,sizeof(rem->h_name)+1)<0){
		perror("write in parse of datagram's port containing buffer");
		exit(1);
	}
	printf("I just wrote to pipe's edge %d the datagram's port %d",fd[WRITE],atoi(dgrport));


	/////////////////////////////////////////      BE CAREFUL   //////////////////////////////////////////////////
	////////////////////////////////////////   3 WRITES IN TOTAL    ///////////////////////////////////////////////
}


main(int argc,char* argv[]){
	//coammand execution - FILE pointers
	FILE *pipe_fp;
	//counter
	int i;
	//probability of sendto failure
	int random;
	//read&write
	int bytesread,retvalue;
	char *command,buffer[BUFSIZE],c;
	//select
	fd_set set,read_set;
	int fd_hwm=0,file_d;   //hwm=high-water-mark=number of fds
	//sockets
	struct sockaddr_in server,client;
	struct sockaddr *serverptr,*clientptr;
	unsigned int serverlen,clientlen;                            //newsock for accept system call
	//signals
	struct sigaction ign_sigpipe;
	struct sigaction sigchld_action; 
	struct sigaction stopall;
	struct sigaction czaction;
	
	//initialize structures in memory
	memset(&ign_sigpipe,0,sizeof(ign_sigpipe)); 
	memset(&sigchld_action,0,sizeof(sigchld_action));
	memset(&stopall,0,sizeof(stopall));
	
	//set handler for cleaning up ONE  child
	sigchld_action.sa_handler=&clean_up_child;   
	sigchld_action.sa_flags=0;                            
	if(sigemptyset(&sigchld_action.sa_mask)<0 || sigaction(SIGUSR1, &sigchld_action, NULL)<0){
		perror("Failed to associate SIGUSR1 with clean_up_child");
		exit(1);
	}
	
	//set handler for killing ALL children + FATHER 
	stopall.sa_handler=time_to_stop;
	stopall.sa_flags=0;
	if(sigemptyset(&stopall.sa_mask)<0 || sigaction(SIGUSR2,&stopall,NULL)<0){
		perror("Failed to associate SIGUSR2 with time_to_stop function");
		exit(1);
	}

	//set handler for killing ALL children + FATHER when Ctrl-C or Ctrl-Z is caught
	czaction.sa_handler=&time_to_stop;			
	czaction.sa_flags=0;                            
	if(sigemptyset(&czaction.sa_mask)<0 || sigaction(SIGINT, &czaction, NULL)<0 || sigaction(SIGTSTP, &czaction, NULL)<0){
		perror("Failed to associate SIGINT or SIGTSTP with czfunction");
		exit(1);
	}

	
	//check for appropriate arguments
	if(argc<4){
		printf("<program_name> <connection_port> <number_of_children> <connection_failure_probability>\n"); 
		perror("args");
		exit(1);
	}
	
	gbl_port=argv[1];
	port=atoi(argv[1]);
	noc=atoi(argv[2]);
	probfail=100*atoi(argv[3]);
	
	//allocate memory for process-id-keeping array
	pid_array=malloc(noc*sizeof(pid_t));
	
	//MANAGE INITIAL COMMUNICATION SOCKET
	sock=socket(PF_INET,SOCK_STREAM,0);
	if(sock==0){
		perror("socket of father process");
		exit(1);
	}
	
	if(setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&socket_option_value,sizeof(socket_option_value))<0){
		perror("Couldn't reinitialize address with setsockopt");
		exit(1);
	}
	
	//initialize sockaddr_in struct for server
	server.sin_family=PF_INET;
	server.sin_addr.s_addr=htonl(INADDR_ANY);
	server.sin_port=htons(port);
	serverptr=(struct sockaddr*)&server;
	serverlen=sizeof(server);
	
	if(bind(sock,serverptr,serverlen)<0){
		perror("bind of father process");
		exit(1);
	}
	
	if(listen(sock,SOMAXCONN)<0){
		perror("listen of father process");
		exit(1);
	}
	
	printf("Listening for TCP connections to port %d...\n",port);
	
	//create pipe
	if(pipe(fd)==-1){
		perror("pipe");
		exit(1);
	}
	//get parent's process id
	parent_id=getpid();
	
	//create noc children
	for(i=0;i<noc;i++){
		if((pid_array[i]=pid=fork())<=0){
			break;
		}
	}
	if(pid==0){                               //child process - actual server
		printf("Child of father with id %ld in execution...PID=%ld\n",parent_id,(long)getpid());
		ign_sigpipe.sa_handler=SIG_IGN;
		ign_sigpipe.sa_flags=0;
		if(sigemptyset(&ign_sigpipe.sa_mask)==-1 || sigaction(SIGPIPE,&ign_sigpipe,NULL)==-1){
			perror("Failed to build action for ignoring SIGPIPE caught by child process");
			exit(1);
		}
		
		do{
			//close not-needed-in-child-mode writing edge of pipe
			close(fd[WRITE]);
			bzero(buffer,sizeof(buffer));
		
			//read dgrport from pipe
			if((bytesread=read(fd[READ],buffer,sizeof(buffer)))<0){
				perror("read DGRPORT from pipe in child");
				exit(1);
			}
		
			port=atoi(buffer);
		
			bzero(buffer,sizeof(buffer));
			if(read(fd[READ],buffer,sizeof(buffer))<0){
				perror("read COMMAND from pipe in child mode");
				exit(1);
			}

			//ignore blanks + define edges of bcopy
			for(i=0;i<512;i++)
				if(buffer[i]=='\n'){
					buffer[i]='\0';
					break;
				}
		
			//create command string	
			bcopy(buffer,command,strlen(buffer)+1);
			
			server.sin_family=PF_INET;
			server.sin_addr.s_addr=htonl(port);
			server.sin_port=htons(port);
			serverptr=(struct sockaddr*)&server;
			serverlen=sizeof(server);
			
			bzero(buffer,sizeof(buffer));	
			if(read(fd[READ],buffer,sizeof(buffer))<0){
				perror("read HOSTNAME from pipe in child mode");
				exit(1);
			}
			
			rem=gethostbyname(buffer);
		
			sock=socket(PF_INET,SOCK_DGRAM,0);  //type=SOCK_DGRAM for UDP connections from/to client
			if(sock==0){
				perror("socket of child");
				exit(1);
			}

			client.sin_family=PF_INET;
			client.sin_addr.s_addr=htonl(INADDR_ANY);
			client.sin_port=htons(port);
			clientptr=(struct sockaddr*)&server;
			clientlen=sizeof(server);
		
			if(bind(sock,clientptr,clientlen)<0){
				perror("bind of child");
				exit(1);
			}
		
			//kill ONE process
			if(strcmp(command,"end")){
				if(kill(getpid(),SIGUSR1)<0){
					perror("Couldn't kill child process on end");
					exit(1);
				}
				close(sock);
				close(newsock);
			}
				
			//kill ALL processes OR kill all processes after client's receival of Ctrl-C or Ctrl-Z signals
			if(strcmp(command,"timeToStop") || strcmp(command,"clientStop")){
				if(kill(getppid(),SIGUSR2)<0){
					perror("Could not intiate killing of all child processes");
					exit(1);
				}
				break;
			}

			//COMMAND EXECUTION//
			if((pipe_fp=popen(command,"r"))==NULL){
				perror("popen for command execution in child");
				exit(1);
			}
			
			bzero(buffer,sizeof(buffer));
			i=0;
			//PROBFAIL DATA LOSS
			srand(time(NULL));
			random=rand()%100+1;
			if(random>1 && random<=probfail)
				while((buffer[i++]=getc(pipe_fp))!=EOF)
					if(sendto(sock,buffer, strlen(buffer)+1, 0, serverptr, serverlen) < 0) {
						perror("sendto of command results");
						exit(1);
					}
			else	
				if(sendto(sock,"DATA LOSS!!!\n It's not your lucky day...sorry mate...\n",100, 0, serverptr, serverlen) < 0) {
					perror("sendto of probability failure message");
					exit(1);
				}
			pclose(pipe_fp);
			//close(newsock);           //-------->created by accept in father mode.close so as to close the connection
		
			close(port);
		}while(strcmp(command,"end"));
		
		
		
		
		
	}

	else{                                     //father process - job distributor and synchronizer
		ign_sigpipe.sa_handler=SIG_IGN;
		ign_sigpipe.sa_flags=0;
		if(sigemptyset(&ign_sigpipe.sa_mask)==-1 || sigaction(SIGPIPE,&ign_sigpipe,NULL)==-1){
			perror("Failed to build action for ignoring SIGPIPE caught by father process");
			exit(1);
		}
		
		//select ready client's file descriptor
		if(sock>fd_hwm)
			fd_hwm=sock;
		FD_ZERO(&set);
		FD_SET(sock,&set);
		while(1){
			read_set=set;
			if(select(FD_SETSIZE,&read_set,NULL,NULL,NULL)<0){
				perror("select");
				exit(1);
			}
			for(file_d=0;file_d<FD_SETSIZE;file_d++)
				if(FD_ISSET(file_d,&read_set)){
					if(file_d==sock){
						clientptr=(struct sockaddr*)&client;
						clientlen=sizeof(client);
						if((newsock=accept(sock,clientptr,&clientlen))<0){
							perror("accept");
							exit(1);
						}
						if((rem=gethostbyaddr((char*)&client.sin_addr.s_addr,sizeof(client.sin_addr.s_addr),client.sin_family))==NULL){
							perror("gethostbyaddr");
							exit(1);
						}
						printf("Accepted connection from %s and port %d\n",rem->h_name,client.sin_port);
						FD_SET(newsock,&set);
						if(newsock>fd_hwm)
							fd_hwm=newsock;
					}
		
					else{
						//client has sent some data so read it
						clientptr = (struct sockaddr*)&client;
						clientlen = sizeof(client);
						if(getpeername(file_d,(struct sockaddr*) &client,&clientlen)<0){
							perror("getpeername father");
							exit(1);
						}
						if((rem=gethostbyaddr((char*)&client.sin_addr.s_addr,sizeof(client.sin_addr.s_addr),client.sin_family)) == NULL){
							perror("gethostbyaddr from getpeername in father");
							exit(1);
						}
						if((retvalue=parse(file_d,rem))==0){
							FD_CLR(file_d,&set);
							if(file_d==fd_hwm)
								fd_hwm--;
							close(file_d);
						}
						
						else if(retvalue<0){
							//part where server sends ready child(file_d=pipe's open fd[READ]) what client's request(buffer) it should accomondate
							printf("Server got command : %s \n",buffer);
							write(file_d,"Too large or unknown command...it will be ignored\n",60);
							close(file_d);
						}
					
						else if(retvalue==1){
							printf("UNSUCCESSFUL CLIENT READING");
							close(file_d);
							exit(1);
						}
						else if(retvalue==2){
							printf("Couldn't read datagram port");
							exit(1);
						}
					}
				}
			}
		}
}
					
					
						


	

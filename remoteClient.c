#include <sys/types.h>		//for sockets
#include <sys/socket.h>		//for sockets
#include <netinet/in.h>		//for Internet sockets
#include <netdb.h>			//for gethostbyname
#include <stdio.h>			//for I/O
#include <stdlib.h>			//for exit
#include <string.h>			//for bzero,bcopy,strlen
#include <signal.h>			//for signal manipulation
#include <errno.h>
#include <sys/time.h>
#define READ 0
#define WRITE 1

int socketoptionvalue=1,retval;
int server_port,receive_port,tcpsock,udpsock,port;
char *buf;
struct hostent *rem;
struct sockaddr_in tcpserver,udpserver,client;
struct sockaddr *tcpserverptr,*udpserverptr,*clientptr;
unsigned int tcpserverlen,udpserverlen,clientlen;
struct sigaction czaction;
struct itimerval timeout1,timeout2;

//timeout2.it_interval.tv_sec;
//timeout2.it_interval.u_sec;
//timeout2.it_value.tv_sec
//timeout2.it_value.u_sec		
	
		
void czfunction(int signo){
	char buf[]="clientStop";
	if(write(tcpsock,buf,strlen(buf)+1)<0){
		perror("czfunction->Error appeared in sendto of clientStop message");
		exit(1);
	}

}

main(int argc,char *argv[]){

	timeout1.it_interval.tv_sec=0;
	timeout1.it_interval.tv_usec=0;
	timeout1.it_value.tv_sec=0;
	timeout1.it_value.tv_usec=1000000;

	//create handler for sending message to server when Ctrl-c or Ctrl-z pressed
	
	czaction.sa_handler=&czfunction;			
	czaction.sa_flags=0;                            
	if(sigemptyset(&czaction.sa_mask)<0 || sigaction(SIGINT, &czaction, NULL)<0 || sigaction(SIGTSTP, &czaction, NULL)<0){
		perror("Failed to associate SIGINT or SIGTSTP with czfunction");
		exit(1);
	}

	if(argc<4){
		printf("Please give server's name and port numbers\n");exit(1);
	}
		
	server_port=atoi(argv[2]);					//convert port number to integer
	receive_port=atoi(argv[3]);					//convert port number to integer
	if((udpsock=socket(PF_INET,SOCK_DGRAM,0))<0){			//create socket
		perror("socket for udp connection");
		exit(1);
	}
	
	if((tcpsock=socket(PF_INET,SOCK_STREAM,0))<0){
		perror("socket for tcp connection");
		exit(1);
	}
	
	if((rem=gethostbyname(argv[1]))==NULL){				//find server address
		perror("gethostbyname");exit(1);
	}
	
	udpserver.sin_family=PF_INET;					//internet domain
	bcopy((char*)rem->h_addr,(char*)&udpserver.sin_addr,rem->h_length);	//copy h_length bytes from h_addr string to sin_addr string(the internet address of server machine)
	udpserver.sin_port=htons(receive_port);				//server's internet address and port
	udpserverptr=(struct sockaddr *) &udpserver;		//adjust &server to struct sockaddr * for sendto and recvfrom functions
	udpserverlen=sizeof udpserver;
	
	tcpserver.sin_family=PF_INET;
	bcopy((char*)rem->h_addr,(char*)&tcpserver.sin_addr,rem->h_length);
	tcpserver.sin_port=htons(server_port);
	tcpserverptr=(struct sockaddr*)&tcpserver;
	tcpserverlen=sizeof tcpserver;
	
	client.sin_family=PF_INET;					//internet domain
	client.sin_addr.s_addr=htonl(INADDR_ANY);	//my internet address
	client.sin_port=htons(receive_port);		//select port
	clientptr=(struct sockaddr *) &client;		//adjust &server to struct sockaddr * for bind function
	clientlen=sizeof client;
	
	if(setsockopt(udpsock,SOL_SOCKET,SO_REUSEADDR,&socketoptionvalue,sizeof(socketoptionvalue))<0){
		perror("setsockopt of udp connection socket");
		exit(1);
	}
	
	if(bind(udpsock,clientptr,clientlen)<0){		//bind socket to address of client
		perror("bind of udp connection");
		exit(1);
	}
	
	if(setsockopt(tcpsock,SOL_SOCKET,SO_REUSEADDR,&socketoptionvalue,sizeof(socketoptionvalue))<0){
		perror("setsockopt of tcp connection socket");
		exit(1);
	}
	
	if(getsockname(udpsock,udpserverptr,&udpserverlen)<0){
		perror("getsockname");
		exit(1);
	}
	
	if(bind(tcpsock,clientptr,clientlen)<0){
		perror("bind of tcp connection");
		exit(1);
	}
	
	while((retval=connect(tcpsock,tcpserverptr,tcpserverlen)==-1)  && (errno==EINTR) );
		if (retval < 0) { /* Request TCP connection */
			perror("connect"); 
			exit(1); }
	printf("Accepted TCP connection to:%s and port no:%d from socket:%d binded to port:%d\n",(argv[1]),server_port,tcpsock,tcpserver.sin_port);

	
	if(write(tcpsock,argv[3],strlen(argv[3])+1)<0){
		perror("write to server failed");
		exit(1);
	}
		
	
	
	printf("Connected with server...\n");
	while(1){		//read messages from stdin
		setitimer(ITIMER_REAL,&timeout1,NULL);
		if(fgets(buf,BUFSIZ,stdin)!=NULL){
			if(write(tcpsock,buf,strlen(buf)+1)<0){
				perror("write to server failed");
				exit(1);
			}
			setitimer(ITIMER_REAL,&timeout1,NULL);

		
			bzero(buf,BUFSIZ);
			if(recvfrom(udpsock,buf,BUFSIZ,0,udpserverptr,&udpserverlen)<0){			//receive message
				perror("recvfrom from server");exit(1);
			}
			printf("%s",buf);						//print received message
		}
	}
}



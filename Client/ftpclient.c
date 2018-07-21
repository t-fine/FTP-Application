//Troy Fine
//CMPE 156
//Programming Project
//./ftpclient 127.0.0.1 1234

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/select.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/uio.h>
#include <time.h>
#include <netdb.h>

#define MAXLINE 100000
#define WAIT 0
#define LIST 1
#define ABOR 2
#define PORT 3
#define QUIT 4
#define RETR 5
#define STOR 6
#define ERROR 7
#define LIMBO 8

//int sendfile(int fd, int s, off_t offset, off_t *len, struct sf_hdtr *hdtr, int flags);
ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
int inet_aton(const char *cp, struct in_addr *inp);
static void* ftpPORT();
static void* ftpLIST();
static void* ftpRETR();
static void* ftpSTOR();

///////////////////////////////
int listenfd, dataFD, bind_status, listen_status, connect_status;
socklen_t clilen;
struct sockaddr_in cliaddr, dataAddr;
int dataPort;
struct sockaddr_in    servaddr;
///////////////////////////////
int controlFD, portNum, inet_status;
int FTP_STATUS = 1;
char recvline[MAXLINE] = {'\0'};
char dataBuf[MAXLINE] = {'\0'};
char controlBuf[MAXLINE] = {'\0'};
char commNotFound[10] = "ERROR";
char fileSize[10] = {"wc -c <"};
char retrievedFile[MAXLINE] = {"retrievedFile.txt"};
char fileNotFound[30] = {"450 Error opening file."};
char port[20] = {"PORT 1234"};
int file_in;
off_t bytes;
//int send_stat;
FILE *fileSTOR, *fileRETR, *output;
time_t rawtime;
char* ftp_comm = NULL;
char* ftp_arg = NULL;
char commandOut[MAXLINE] = {'\0'};
size_t send_stat;


int main (int argc, char * argv[]){
	
	char sendline[MAXLINE] = {'\0'};
	
	pthread_t tid;
	
	
	///////////////////////////CHECK COMMAND-LINE ARGUMENTS////////////////////////////
	if(argc != 3){
		printf("Usage: Server IP address and listening port number required.\n");
		exit(1);
	}
	
	///////////////////////////SOCKET SETUP: SOCKET, CONNECT///////////////////////////
	controlFD = socket(AF_INET, SOCK_STREAM, 0);
	if (controlFD < 0){
		printf("socket(): error.\n");
		exit(1);
	}
	else printf("socket(): success!\n");
	
	portNum = atoi(argv[2]); //get port number from char to int
	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	//servaddr.sin_addr.s_addr = htonl(argv[1]);
	servaddr.sin_port = htons(portNum); //SERV_PORT
	
	////////////SET MY CLIENT PORT NUM///////////////
	//pass in current time as seed;
	time ( &rawtime );
	int randPort = (rawtime + 1000)% (65000 + 1 - 1000);
		
	bzero(&cliaddr, sizeof(cliaddr));
	cliaddr.sin_family      = AF_INET;
	cliaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	cliaddr.sin_port        = htons(randPort); //randPort
	
	bind_status = bind(controlFD, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
	if (bind_status < 0){
		printf("bind(): error.\n");
		exit(1);
	}
	else printf("bind(): success!\n");
	
	
	dataPort = htons(randPort - 1);
	printf("My port number is: %d\n", dataPort);
	
	///////////////////////////////////////
	inet_status = inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
	if (inet_status < 0){
		printf("inet_pton(): error.\n");
		exit(1);
	}
	else if (inet_status == 0){
		printf("inet_pton(): error invalid presentation error.\n");
		exit(1);
	}
	else printf("inet_pton(): success!\n");
	
	connect_status = connect(controlFD, (struct sockaddr *) &servaddr, sizeof(servaddr));
	if (connect_status < 0){
		printf("connect(): error\n");
		exit(1);
	}
	else printf("connect(): success!\n\n");
	
	
	write(controlFD, port, strlen(port));
	FTP_STATUS = WAIT;
	pthread_create(&tid, NULL, &ftpPORT, NULL);
	
	for(int i = 0;;++i){
		while(FTP_STATUS == WAIT){};
		printf("ftp> ");
		
		fgets(sendline, sizeof(sendline), stdin);
		strcpy(commandOut, sendline);
		ftp_comm = strtok(sendline, " \n");
		ftp_comm [strcspn(ftp_comm, "\r\n")] = '\0';
		
		if((strcmp(ftp_comm, "quit") == 0) || (strcmp(ftp_comm, "ls") == 0)){
			if(strcmp(ftp_comm, "quit") == 0){
				write(controlFD, commandOut, strlen(commandOut));
				FTP_STATUS = WAIT;
				read(controlFD, recvline, MAXLINE);
				printf("%s\n", recvline);
				goto exit;
				
			}
			if(strcmp(ftp_comm, "ls") == 0){
				write(controlFD, commandOut, strlen(commandOut));
				FTP_STATUS = WAIT;
				pthread_create(&tid, NULL, &ftpLIST, NULL);
				
			}
			
		}
		else if((strcmp(ftp_comm, "get") == 0) || (strcmp(ftp_comm, "put") == 0)){
			ftp_arg = strtok(NULL, " \n");
			ftp_arg [strcspn(ftp_arg, "\r\n")] = '\0';
			
			if(strcmp(ftp_comm, "get") == 0){
				write(controlFD, commandOut, strlen(commandOut));
				FTP_STATUS = WAIT;
				pthread_create(&tid, NULL, &ftpRETR, NULL);
				//write(&sockPtr, replyQuit, sizeof(replyQuit));
				//exit(221);
			}
			if(strcmp(ftp_comm, "put") == 0){
				//write(controlFD, commandOut, strlen(commandOut));
				FTP_STATUS = WAIT;
				pthread_create(&tid, NULL, &ftpSTOR, NULL);
				//write(&sockPtr, replyQuit, sizeof(replyQuit));
				//exit(221);
			}
		}
		else{
			write(controlFD, commNotFound, strlen(commNotFound));
			read(controlFD, recvline, MAXLINE);
			printf("%s\n", recvline);
		}
		
	}
	
	//////////////////////////////CLOSE FILE AND SOCKET///////////////////////////////
exit:
	close(controlFD);
	close(dataFD);
	close(controlFD);
	exit(0);
}

/////////////////////////////OPENING PORT FOR DATA TRANSFER/////////////////////////
static void* ftpPORT(){
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd < 0){
		printf("DATA socket(): error.\n");
		exit(1);
	}
	//else printf("DATA socket(): success!\n");
	
	cliaddr.sin_port = dataPort;
	
	bind_status = bind(listenfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
	if (bind_status < 0){
		printf("DATA bind(): error.\n");
		exit(1);
	}
	//else printf("DATA bind(): success!\n");
	
	listen_status = listen(listenfd, 5);
	if (listen_status < 0){
		printf("DATA listen(): error.\n");
		exit(1);
	}
	//else printf("DATA listen(): success!\n");
	
	clilen = sizeof(servaddr);
	
	dataFD = accept(listenfd, (struct sockaddr *) &servaddr, &clilen);
	if (dataFD < 0){
		printf("DATA accept(): error.\n");
		exit(1);
	}
	//else printf("DATA accept(): success!\n");
	read(controlFD, controlBuf, MAXLINE);
	printf("%s\n", controlBuf);
	FTP_STATUS = PORT;
	return(NULL);
}

////////////////////////LISTING FILES IN SERVERS DIRECTORY///////////////////
static void* ftpLIST(){
	read(dataFD, dataBuf, MAXLINE);
	printf("%s\n", dataBuf);
	
	read(controlFD, controlBuf, MAXLINE);
	printf("%s\n", controlBuf);
	FTP_STATUS = LIST;
	return(NULL);
}

//////////////////////RETRIEVING FILE FROM SERVER/////////////////////////
static void* ftpRETR(){
	read(controlFD, controlBuf, MAXLINE);
	printf("%s\n", controlBuf);
	
	if(strcmp(controlBuf, "450 Error opening file.") == 0){
		goto exit;
		
	}
	
	read(dataFD, dataBuf, MAXLINE);
	//printf("%s\n", recvline);
	//fileRETR = fopen(retrievedFile, "w");
	fileRETR = fopen(ftp_arg, "w");
	if(fileRETR == NULL){
		//printf("Unable to open file %s for reading\n", ftp_arg);
		goto exit;
	}
	
	
	fwrite(dataBuf, sizeof(char), MAXLINE, fileRETR);
	fclose(fileRETR);
exit:
	FTP_STATUS = LIST;
	return(NULL);
}

/////////////////////////////STORING FILE TO SERVER///////////////////////////////
static void* ftpSTOR(){
	//read(dataFD, recvline, MAXLINE);
	//printf("%s\n", recvline);
	
	fileSTOR = fopen(ftp_arg, "r");
	if(fileSTOR == NULL){
		//printf("Unable to open file %s for reading\n", ftp_arg);
		//write(dataFD, fileNotFound, sizeof(fileNotFound));
		printf("%s\n", fileNotFound);
		goto exit;
	}
	write(controlFD, commandOut, strlen(commandOut));
	file_in = fileno(fileSTOR);
	
	
	//send_stat = sendfile(file_in, dataFD, 0, &bytes, 0, 0);
	send_stat = sendfile(dataFD, file_in, 0, &bytes);
	read(controlFD, controlBuf, MAXLINE);
	printf("%s\n", controlBuf);
	fclose(fileSTOR);
exit:
	FTP_STATUS = LIST;
	return(NULL);
}

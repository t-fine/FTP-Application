//Troy Fine
//CMPE 156
//Programming Project
//./ftpserver 1234

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
FILE *popen(const char *command, const char *type);
int pclose(FILE *stream);
int fileno(FILE *stream);
static void* ftpPORT();
static void* ftpLIST();
static void* ftpRETR();
static void* ftpSTOR();

char* cliIP;
int cliPort;
char* ftp_arg;
struct sockaddr_in cliaddr, servaddr;
struct sockaddr_in addr;
int dataFD;
int controlFD;
int FTP_STATUS = 1;
FILE *fileSTOR, *fileRETR, *output;
char commOutput[MAXLINE] = {'\0'};
char ftpOutput[MAXLINE] = {'\0'};
char fileSize[10] = {"wc -c <"};
char getFileSize[MAXLINE] = {'\0'};
char fileData[MAXLINE] = {'\0'};
char storedFile[MAXLINE] = {"storedFile.txt"};
char fileNotFound[30] = {"450 Error opening file."};
char dataConnError[50] = {"452 Can't open data connection."};
char fileWriteError[30] = {"450 Error writing file."};
char commOK[20] = {"220 Command OK."};
char dataBuf[MAXLINE] = {'\0'};
char controlBuf[MAXLINE] = {'\0'};

int file_in;
off_t bytes;
//int send_stat;
size_t send_stat;


int main (int argc, char * argv[]){
	int listenfd, portNum, bind_status, listen_status;
	socklen_t clilen;
	
	char buff[MAXLINE] = {'\0'};
	char readline[MAXLINE] = {'\0'};
	//char commCheck[MAXLINE] = {'\0'};
	char replyQuit[20] = {"221 Goodbye."};
	char commNotFound[20] = {"500 Syntax Error."};
	char* ftp_comm;
	pthread_t tid;
	char* stringIP = "127.0.0.1";
	
	
	///////////////////////////CHECK COMMAND-LINE ARGUMENTS////////////////////////////
	if(argc != 2){
		printf("Usage: Server listening port number required.\n");
		exit(1);
	}
	
	///////////////////SOCKET SETUP: SOCKET, BIND, LISTEN, ACCEPT//////////////////////

	
	inet_aton(stringIP, (struct in_addr*)&addr);
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd < 0){
		printf("socket(): error.\n");
		exit(1);
	}
	else printf("socket(): success!\n");
	
	portNum = atoi(argv[1]);
	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(addr.sin_addr.s_addr);
	servaddr.sin_port        = htons(portNum);
	
	bind_status = bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	if (bind_status < 0){
		printf("bind(): error.\n");
		exit(1);
	}
	else printf("bind(): success!\n");
	
newClient:
	listen_status = listen(listenfd, 5);
	if (listen_status < 0){
		printf("listen(): error.\n");
		exit(1);
	}
	else printf("listen(): success!\n");
	
	/////////////////////////////THREADS TO DEAL WITH MORE THAN ONE CLIENT///////////////////
	clilen = sizeof(cliaddr);
	
	//sockPtr = malloc(sizeof(int));
	controlFD = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
	if (controlFD < 0){
		printf("accept(): error.\n");
		exit(1);
	}
	else printf("accept(): success!\n");
	
	////////////////CLIENTS IP AND PORT//////////////
	cliIP = inet_ntop(AF_INET, &cliaddr.sin_addr, buff, sizeof(buff));
	cliPort = ntohs(cliaddr.sin_port);
	
	
	printf("Connection from IP: %s, on port: %d\n\n", cliIP, cliPort);
	
	for(;;){
		while(FTP_STATUS == WAIT){};
		memset(readline, '\0', MAXLINE);
		memset(fileData, '\0', MAXLINE);
		read(controlFD, readline, MAXLINE);
		ftp_comm = strtok(readline, " \n");
		ftp_comm [strcspn(ftp_comm, "\r\n")] = '\0';
		if((strcmp(ftp_comm, "quit") == 0) || (strcmp(ftp_comm, "ls") == 0)){//|| (strcmp(ftp_comm, "ABOR") == 0)
			if(strcmp(ftp_comm, "quit") == 0){
				FTP_STATUS = QUIT;
				write(controlFD, replyQuit, sizeof(replyQuit));
				goto newClient;
				//exit(221);
			}
			if(strcmp(ftp_comm, "ls") == 0){
				FTP_STATUS = WAIT;
				pthread_create(&tid, NULL, &ftpLIST, NULL);
			}
			/*
			if(strcmp(ftp_comm, "ABOR") == 0){
				FTP_STATUS = WAIT;
				printf("ABOR commend\n");
				//pthread_create(&tid, NULL, &ftpLIST, NULL);
			}
			*/
			
		}
		else if((strcmp(ftp_comm, "PORT") == 0) || (strcmp(ftp_comm, "get") == 0) || (strcmp(ftp_comm, "put") == 0)){
			ftp_arg = strtok(NULL, " \n");
			ftp_arg [strcspn(ftp_arg, "\r\n")] = '\0';
			if(strcmp(ftp_comm, "PORT") == 0){
				//printf("PORT\n");
				FTP_STATUS = WAIT;
				pthread_create(&tid, NULL, &ftpPORT, NULL);
				//write(&sockPtr, replyQuit, sizeof(replyQuit));
				//exit(221);
			}
			if(strcmp(ftp_comm, "get") == 0){
				//printf("retr\n");
				FTP_STATUS = WAIT;
				pthread_create(&tid, NULL, &ftpRETR, NULL);
				//write(&sockPtr, replyQuit, sizeof(replyQuit));
				//exit(221);
			}
			if(strcmp(ftp_comm, "put") == 0){
				//printf("stor\n");
				FTP_STATUS = WAIT;
				pthread_create(&tid, NULL, &ftpSTOR, NULL);
				//write(&sockPtr, replyQuit, sizeof(replyQuit));
				//exit(221);
			}
			
			
		}
		else{
			write(controlFD, commNotFound, sizeof(commNotFound));
			FTP_STATUS = ERROR;
		}
		
	}
	fclose(fileRETR);
	fclose(fileSTOR);
	close(dataFD);
	close(controlFD);
	return (0);
}

//////////////////////CONNECTING TO CLIENTS LISTENING DATA TRANSFER PORT////////////////
static void* ftpPORT(){
	int inet_status, connect_status;
	
	//printf("Welcome to the FTT Server Thread!\n");
	
	dataFD = socket(AF_INET, SOCK_STREAM, 0);
	if (dataFD < 0){
		printf("DATA socket(): error.\n");
		write(controlFD, dataConnError, sizeof(dataConnError));
		goto exit;
	}
	//else printf("DATA socket(): success!\n");
	
	inet_aton(cliIP, (struct in_addr*)&addr);
	
	bzero(&servaddr, sizeof(servaddr));
	cliaddr.sin_family = AF_INET;
	cliaddr.sin_addr.s_addr = htonl(addr.sin_addr.s_addr);
	//cliaddr.sin_addr.s_addr = cliIP;
	cliaddr.sin_port = htons(cliPort - 1); //SERV_PORT
	
	///////////////////////////////////////
	inet_status = inet_pton(AF_INET, cliIP, &cliaddr.sin_addr);
	if (inet_status < 0){
		printf("DATA inet_pton(): error.\n");
		write(controlFD, dataConnError, sizeof(dataConnError));
		goto exit;
	}
	else if (inet_status == 0){
		printf("DATA inet_pton(): error invalid presentation error.\n");
		write(controlFD, dataConnError, sizeof(dataConnError));
		goto exit;
	}
	//else printf("DATA inet_pton(): success!\n");
	
	sleep(3);
	connect_status = connect(dataFD, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
	if (connect_status < 0){
		printf("DATA connect(): error\n");
		write(controlFD, dataConnError, sizeof(dataConnError));
		goto exit;
	}
	else printf("Data connection success.\n");
	
	write(controlFD, commOK, sizeof(commOK));
exit:
	FTP_STATUS = PORT;
	return(NULL);
}

////////////////////////LISTING FILES IN CURRENT DIRECTORY///////////////////
static void* ftpLIST(){
	memset(commOutput, '\0', MAXLINE);
	memset(ftpOutput, '\0', MAXLINE);
	output = popen("ls", "r");
	while((fgets(commOutput, sizeof(commOutput), output)) != NULL){
		strcat(ftpOutput, commOutput);
	}
	pclose(output);
	
	write(dataFD, ftpOutput, sizeof(ftpOutput));
	
	write(controlFD, commOK, sizeof(commOK));
	FTP_STATUS = LIST;
	return(NULL);
}

//////////////////////SENDING FILE TO CLIENT/////////////////////////
static void* ftpRETR(){
	fileRETR = fopen(ftp_arg, "r");
	if(fileRETR == NULL){
		printf("Unable to open file %s for reading\n", ftp_arg);
		write(controlFD, fileNotFound, sizeof(fileNotFound));
		goto exit;
	}
	file_in = fileno(fileRETR);
	
	write(controlFD, commOK, sizeof(commOK));
	
	//send_stat = sendfile(file_in, dataFD, 0, &bytes, 0, 0);
	send_stat = sendfile(dataFD, file_in, 0, &bytes);
	
	
	fclose(fileRETR);
exit:
	FTP_STATUS = LIST;
	return(NULL);
}

////////////////////////STORING FILE FROM CLIENT///////////////////////
static void* ftpSTOR(){
	read(dataFD, fileData, MAXLINE);
	if(strcmp(fileData, "450 Error opening file.") == 0){
		memset(fileData, '\0', MAXLINE);
		goto exit;
	}
	fileSTOR = fopen(ftp_arg, "w");
	if(fileSTOR == NULL){
		printf("Unable to open file %s for reading\n", ftp_arg);
		write(controlFD, fileWriteError, sizeof(fileWriteError));
		goto exit;
	}
	
	fwrite(fileData, sizeof(char), MAXLINE, fileSTOR);
	write(controlFD, commOK, sizeof(commOK));
	fclose(fileSTOR);
exit:
	FTP_STATUS = LIST;
	return(NULL);
}


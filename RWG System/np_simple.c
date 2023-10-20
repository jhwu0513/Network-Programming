#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <netinet/in.h>

#define MaxPipe 1001
#define Process_Number 512
#define MAX_CMD_LEN 15001

int process[Process_Number];
int file_fd[MaxPipe][2];
int PC = -1;
int Pindex = -1;
int NoNumPipe;
int SERV_TCP_PORT;

void Synchronization(){
    for (int i = 0; i < Process_Number; i++)
        if(process[i] != -1)
        {
            waitpid(process[i], NULL, 0);
            process[i] = -1;
        }
}

void IniTable(){
	for (int i = 0; i < MaxPipe; i++){
		file_fd[i][0] = -1;
		file_fd[i][1] = -1;
	}
	for (int i = 0; i < Process_Number; i++) process[i] = -1;
}

int Number_Pipe(char *s){
	if((s[0]!='|' && s[0]!='!') || strlen(s)<2) return 0;
	for(int i=1;i<strlen(s);i++)
		if(!isdigit(s[i])) return 0;
	return 1;
}

int CMD(char **arg, int *fd, int Command_Mode, int STDIN, int STDOUT)
{
	int pid;
	/*
		Command_Mode :
			0:代表該行只有1個指令，可能是單一普通指令，也有可能有File Redirection
			1:代表遇到Ordinary Pipe
			2:代表遇到|的Number Pipe
			3:代表遇到!的Number Pipe
	*/
	while ((pid = fork()) < 0) sleep(1000);
	if (pid == 0)
	{
		if (Command_Mode)
		{
			if (Command_Mode == 3) dup2(fd[1], 2);
			dup2(fd[1], 1);
			close(fd[0]);
			close(fd[1]);
		}
		if (execvp(arg[0], arg) == -1)
		{
			fprintf(stderr, "Unknown command: [%s].\n", arg[0]);
			exit(EXIT_FAILURE);
		}
	}
	else if (pid > 0)
	{
		if(NoNumPipe){
			Pindex = (Pindex + 1) % Process_Number;
			process[Pindex] = pid;
		}
		if (Command_Mode == 1)
		{
			dup2(fd[0],0);
			close(fd[0]);
			close(fd[1]);
		}
		else dup2(STDIN, 0);
	}
	if (Command_Mode != 1) free(arg);
	return 1;
}

int NoNumPipeFunc(char command[]){
	char line[strlen(command)+1];
	strcpy(line, command);
	char *word = strtok(line, " \n");
	while (word)
	{
		if(Number_Pipe(word)) return 0;
		word = strtok(NULL, " \n");
	}
	return 1;
}

int ReadLine(int STDIN,char *s,char p[][MAX_CMD_LEN]) //回傳值代表有幾個指令
{
	// recv(STDIN,s, MAX_CMD_LEN,0);
	// for(int i=0;i<strlen(s);i++)
	// 	if((int)s[i]==13 || (int)s[i]==10) s[i]='\0';
    char *a = strtok(s," \n");
	if(!a) return 0;
    int cmd=1,flag=0;
    while(a){
        if (Number_Pipe(a)){
            strcat(p[cmd-1],a);
            flag=1;
        }
        else{
			if(flag) cmd++;
            strcat(p[cmd-1],a);
            strcat(p[cmd-1]," ");
			flag=0;
        }
        a = strtok(NULL, " \n");
    }
	return cmd;
}


int LineParser(char Line[], int STDIN, int STDOUT)
{
	char cp[strlen(Line)+1];
	strcpy(cp, Line);
	char **arg = malloc(50 * sizeof(char *));
	memset(arg,'\0',50);
	if (file_fd[PC][0] != -1)
	{
		dup2(file_fd[PC][0], 0);
		close(file_fd[PC][0]);
		close(file_fd[PC][1]);
		file_fd[PC][0] = -1;
		file_fd[PC][1] = -1;
	}
	int cnt = 0; 
	char *word = strtok(cp, " \n");
	while (word)
	{
		/*
		如果沒遇到Ordinary Pipe、Number Pipe、File Redirection
		就把所有參數收集起來
		如果遇到Ordinary Pipe代表新的指令要執行
		先將舊的指令執行完，再重新收集參數(cnt=0)
		*/
		if (!strcmp(word,"|")) //如果看到Ordinary Pipe
		{
			int fd[2];
			pipe(fd);
			CMD(arg, fd, 1, STDIN, STDOUT);
			memset(arg,'\0',50);
			cnt = 0;
		}
		else if (!strcmp(word, ">"))
		{
			word = strtok(NULL, " \n");
			int file;
			file = open(word,O_RDWR | O_CREAT | O_TRUNC ,S_IRWXU | S_IRWXG | S_IRWXO,0777); // 可以改改看
			dup2(file, 1);
			CMD(arg, NULL, 0, STDIN, STDOUT);
			dup2(STDOUT, 1);
			return 1;
		}
		else if(!strcmp(word, ">>")){
			// printf("true\n");
			word = strtok(NULL, " \n");
			int file;
			file = open(word,O_RDWR | O_CREAT | O_APPEND);
			dup2(file, 1);
			CMD(arg, NULL, 0, STDIN, STDOUT);
			dup2(STDOUT, 1);
			return 1;
		}
		else if(Number_Pipe(word))  // 他是number pipe
		{
			int Pipe_N = atoi(&word[1]);
			int addr = (PC + Pipe_N) % MaxPipe;
			if (file_fd[addr][1] == -1) pipe(file_fd[addr]);
			if (word[0] == '!') CMD(arg, file_fd[addr], 3, STDIN, STDOUT);
			else CMD(arg, file_fd[addr], 2, STDIN, STDOUT);
            sleep(1000);
			return 2;
		}
		else arg[cnt++] = word;
		word = strtok(NULL, " \n");
	}
	if(!strcmp(arg[0],"printenv")){
		if(getenv(arg[1])){
			write(STDOUT,getenv(arg[1]),strlen(getenv(arg[1])));
			write(STDOUT,"\n",strlen("\n"));
		}
		free(arg);
		return 1;
	}
	else if(!strcmp(arg[0],"setenv")){
		setenv(arg[1], arg[2], 1);
		free(arg);
		return 1;
	}
	else if(!strcmp(arg[0],"exit")){
		free(arg);
		exit(0);
	}
	/*
	這裡是為了執行Ordinary Pipe最後的指令
	*/
	CMD(arg,NULL,0,STDIN, STDOUT);
	return 1;
}

void sig_handler(int signo)
{
	int status;
	wait(&status);
}

int shell(int fd) //回傳值代表有幾個指令
{
	int STDIN = dup(0);
	int STDOUT = dup(1); //用來recovery
	int next = -1;
	IniTable();
	signal(SIGCHLD | SIGINT, sig_handler);
    setenv("PATH", "bin:.", 1);
	while(next){
		write(fd,"% ",strlen("% "));
		char s[MAX_CMD_LEN];
		recv(fd,s, MAX_CMD_LEN,0);
		for(int i=0;i<strlen(s);i++)
			if((int)s[i]==13 || (int)s[i]==10) s[i]='\0';
		char p[100][MAX_CMD_LEN];
		for(int i=0;i<100;i++) strcpy(p[i],"");
		int cmd = ReadLine(STDIN,s,p);
		if(!cmd) next = 1;
        for(int i=0;i<cmd;i++){
			PC = (PC + 1) % MaxPipe;
			NoNumPipe = NoNumPipeFunc(p[i]);
            next = LineParser(p[i], STDIN, STDOUT);
			/*
			next:
				0:exit
				1:當行一定會執行完指令(File Redirection、Ordinary Pipe)
				2.:那行有Number Pipe，有輸出還沒搞定
			*/
			if(next==1) Synchronization(); // 用來對齊%，等Stdout，也就是等會執行完的Process
        }
	}
}

int DisplayCMD(int fd) //回傳值代表有幾個指令
{
	while(1){
		write(fd,"% ",strlen("% "));
		char s[MAX_CMD_LEN];
		recv(fd,s, MAX_CMD_LEN,0);
		for(int i=0;i<strlen(s);i++)
			if((int)s[i]==13 || (int)s[i]==10) s[i]='\0';
		write(fd,s,strlen(s));
		write(fd,"\n",strlen("\n"));
		// for(int i=0;i<(int)strlen(s)+1;i++){
		// 	char p[100];
		// 	sprintf(p,"%c:%d\n",s[i],(int)s[i]);
		// 	write(fd,p,strlen(p));
		// }
	}
}

int main(int argc , char *argv[])
{
	signal(SIGCHLD, sig_handler);
	int STDIN = dup(0),STDOUT = dup(1),STDERR = dup(2);
	SERV_TCP_PORT = atoi(argv[1]); //第一個參數假設是port
	int sockfd,newsockfd,clilen,childpid;
	struct sockaddr_in cli_addr,serv_addr;
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	bzero((char *)&serv_addr,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); //任何網卡位址都可以連
	serv_addr.sin_port = htons(SERV_TCP_PORT);
	int optval;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	bind(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
	listen(sockfd,1);
	while(1){
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd,(struct sockaddr *)&cli_addr,&clilen);
		dup2(newsockfd,0);
		dup2(newsockfd,1);
		dup2(newsockfd,2);
		childpid = fork();
		if(!childpid){
			close(sockfd);
			shell(newsockfd);
			// DisplayCMD(newsockfd);
			exit(0);
		}
		dup2(STDIN,0);
		dup2(STDOUT,1);
		dup2(STDERR,2);
		close(newsockfd);
	}
	return 0;
}

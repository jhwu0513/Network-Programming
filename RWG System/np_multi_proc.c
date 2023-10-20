#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MaxPipe 1001
#define Process_Number 512
#define MAX_CMD_LEN 15001
#define MAX_MESSAGE 1025
#define SEMKEY ((key_t) 7890)
#define SEMKEY1 ((key_t) 7891)
#define SEMKEY2 ((key_t) 7892)
#define Max_Clients 30

int process[Process_Number];
int file_fd[MaxPipe][2];
int PC = -1;
int Pindex = -1;
int NoNumPipe;
int SERV_TCP_PORT;
int shm_id;
int uid = 0;
int devnull;


typedef struct Client{
	int cli_fd;
	struct sockaddr_in cli_addr;
	int id;
	int pid;
	int user_pipe_fd[Max_Clients][2];
	char mesg_data[MAX_MESSAGE];
	char name[100];
}Client;

Client *cli_table;

void Broadcast(char s[]){
	for(int i=0;i<Max_Clients;i++){
		if(cli_table[i].cli_fd){
			memset(cli_table[i].mesg_data,'\0',MAX_MESSAGE);
			strcpy(cli_table[i].mesg_data,s);
			kill(cli_table[i].pid,SIGUSR1);
		}
	}
}

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

int Input_from_User_Pipe(int fd[],int id,char command[]){
	char s[500];
	if(id<=0 || id>30 || !cli_table[id-1].cli_fd){
		sprintf(s, "*** Error: user #%d does not exist yet. ***\n", id);
		Broadcast(s);
		return 1;
	}
	char FIFO[100];
	sprintf(FIFO,"user_pipe/%d-%d",id,uid);
	cli_table[uid-1].user_pipe_fd[id-1][0] = open(FIFO,O_RDONLY | O_NONBLOCK);
	if(cli_table[uid-1].user_pipe_fd[id-1][1]< 0)
	{
		sprintf(s, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", id, uid);
		write(1,s,strlen(s));
		return 1;
	}
	else{
		sprintf(s, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", cli_table[uid-1].name, uid, cli_table[id-1].name, id, command);
		Broadcast(s);
		dup2(cli_table[uid-1].user_pipe_fd[id-1][0], 0);
		close(cli_table[uid-1].user_pipe_fd[id-1][0]);
		unlink(FIFO);
		cli_table[uid-1].user_pipe_fd[id-1][0] = -1;
		cli_table[uid-1].user_pipe_fd[id-1][1] = -1;
	}
	return 0;
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
		if(!strcmp(word, "yell"))
		{
			arg[cnt++] = word;
			word = strtok(NULL, "\n"); //get msg
			arg[cnt++] = word;
			break;	
		}
		if(!strcmp(word, "name"))
		{
			arg[cnt++] = word;
			word = strtok(NULL, "\n"); //get msg
			arg[cnt++] = word;
			break;	
		}
		else if(!strcmp(word, "tell"))
		{
			arg[cnt++] = word;
			word = strtok(NULL, " "); //get user id
			arg[cnt++] = word;
			word = strtok(NULL, "\n"); //get msg
			arg[cnt++] = word;
			break;
		}
		else if(word[0] == '>' && strlen(word) > 1){
			int target = atoi(&word[1]);
			char s[500];
			char *input = strtok(NULL, " \n");
			if(input && input[0] == '<' && strlen(input) > 1){
				int flag = Input_from_User_Pipe(cli_table[uid-1].user_pipe_fd[atoi(&input[1])-1],atoi(&input[1]),Line); // 從這裡獲取Input，並且把0給dup掉
				if(flag) dup2(devnull,0); // 如果沒有東西可以當作輸入端
			}
			if(target<=0 || target>30 || !cli_table[target-1].cli_fd){
				sprintf(s, "*** Error: user #%d does not exist yet. ***\n", target);
				write(1,s,strlen(s));
				dup2(devnull, 1);
				break;
			}
			if(cli_table[target-1].user_pipe_fd[uid-1][1] >= 0)
			{
				sprintf(s, "*** Error: the pipe #%d->#%d already exists. ***\n", uid, target);
				write(1,s,strlen(s));
				dup2(devnull, 1);
			}
			else{
				// pipe(cli_table[target-1].user_pipe_fd[user->id-1]);
				char FIFO[100];
				sprintf(FIFO,"user_pipe/%d-%d",uid,target);
				sprintf(s, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", cli_table[uid-1].name, uid, Line, cli_table[target-1].name, target);
				Broadcast(s);
				if(mkfifo(FIFO,0666)<0)
					write(1,"FIFO Error.\n",strlen("FIFO Error.\n"));
				cli_table[target-1].user_pipe_fd[uid-1][0] = open(FIFO,O_RDONLY | O_NONBLOCK);
				if((cli_table[target-1].user_pipe_fd[uid-1][1] = open(FIFO,O_WRONLY))<0)
					write(1,"FIFO open Error.\n",strlen("FIFO open Error.\n"));
				dup2(cli_table[target-1].user_pipe_fd[uid-1][1], 1); // 可能會造成大問題，要把1復原成cli_fd
				close(cli_table[target-1].user_pipe_fd[uid-1][1]);
			}
			break; //可能會有 >2 <1這種case，所以執行完就要break
		}
		else if(word[0] == '<' && strlen(word) > 1){
			int flag = Input_from_User_Pipe(cli_table[uid-1].user_pipe_fd[atoi(&word[1])-1],atoi(&word[1]),Line);
			if(flag) dup2(devnull,0);
		}
		else if (!strcmp(word,"|")) //如果看到Ordinary Pipe
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
	if(!strcmp(arg[0],"who")){
		char p[100]="<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
		write(STDOUT,p,strlen(p));	
		for(int i=0; i<Max_Clients; i++)
			if(cli_table[i].cli_fd != 0)
			{    
				char s[100];
				char p[200];
				sprintf(s, "%s:%d", inet_ntoa(cli_table[i].cli_addr.sin_addr), ntohs(cli_table[i].cli_addr.sin_port));
				if(uid == cli_table[i].id){
					sprintf(p,"%d\t%s\t%s\t<-me\n", cli_table[i].id, cli_table[i].name, s);
					write(STDOUT,p,strlen(p));
				}
				else{
					sprintf(p,"%d\t%s\t%s\t\n", cli_table[i].id, cli_table[i].name, s);
					write(STDOUT,p,strlen(p));
				}
			}
		free(arg);
		return 1;
	}
	else if(!strcmp(arg[0],"name")){
		int i;
		char s[200];
		for(i=0; i<Max_Clients; i++)
		{
			if(cli_table[i].cli_fd > 0 && !strcmp(cli_table[i].name, arg[1]))
			{    
				sprintf(s, "*** User '%s' already exists. ***\n", arg[1]);
				write(cli_table[uid-1].cli_fd,s,strlen(s));
				break;
			}
		}
		if(i==Max_Clients){
			strcpy(cli_table[uid-1].name,arg[1]);
			memset(s,'\0',200);
			sprintf(s, "*** User from %s:%d is named '%s'. ***\n", inet_ntoa(cli_table[uid-1].cli_addr.sin_addr), ntohs(cli_table[uid-1].cli_addr.sin_port),cli_table[uid-1].name);
			// write(1,s,sizeof(s));
			Broadcast(s);
		}
		free(arg);
		return 1;
	}
	else if(!strcmp(arg[0],"yell")){
		char s[1500];
		sprintf(s, "*** %s yelled ***: %s\n", cli_table[uid-1].name, arg[1]);
		Broadcast(s);
		free(arg);
		return 1;
	}
	else if(!strcmp(arg[0],"tell")){
		char s[1500];
		int flag = 1;
		for(int i=0;i<Max_Clients;i++)
			if(cli_table[i].id==atoi(arg[1]) && cli_table[i].cli_fd!=0){
				memset(cli_table[i].mesg_data,'\0',MAX_MESSAGE);
				sprintf(cli_table[i].mesg_data, "*** %s told you ***: %s\n", cli_table[uid-1].name, arg[2]);
				kill(cli_table[i].pid,SIGUSR1);
				flag = 0;
				break;
			}
		if(flag){
			sprintf(s, "*** Error: user #%s does not exist yet. ***\n", arg[1]);
			write(cli_table[uid-1].cli_fd,s,strlen(s));
		}
		free(arg);
		return 1;
	}
	else if(!strcmp(arg[0],"printenv")){
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
		close(cli_table[uid-1].cli_fd);
		cli_table[uid-1].cli_fd = 0;
		cli_table[uid-1].id = 0;
		cli_table[uid-1].pid = 0;
		char FIFO[100];
		for(int i=0;i<Max_Clients;i++){
			cli_table[uid-1].user_pipe_fd[i][0] = -1;
			cli_table[uid-1].user_pipe_fd[i][1] = -1;
			sprintf(FIFO,"user_pipe/%d-%d",i+1,uid);
			unlink(FIFO);
		}
		char s[200];
		sprintf(s, "*** User '%s' left. ***\n", cli_table[uid-1].name);
		Broadcast(s);
		strcpy(cli_table[uid-1].name,"");
		strcpy(cli_table[uid-1].mesg_data,"");
		shmdt(cli_table);
		free(arg);
		exit(0);
	}
	/*
	這裡是為了執行Ordinary Pipe最後的指令
	*/
	CMD(arg,NULL,0,STDIN, STDOUT);
	dup2(cli_table[uid-1].cli_fd, 1);
	dup2(cli_table[uid-1].cli_fd, 0);
	return 1;
}

void sig_handler(int signo)
{
	int status;
	wait(&status);
}

void msg_handler(int signo)
{
	write(1,cli_table[uid-1].mesg_data,strlen(cli_table[uid-1].mesg_data));
}

void ipc_handler(int signo)
{
	for(int i=0;i<Max_Clients;i++){
		cli_table[i].cli_fd = 0;
		cli_table[i].id = 0;
		cli_table[i].pid = 0;
		strcpy(cli_table[i].name,"");
		strcpy(cli_table[i].mesg_data,"");
		for(int j=0;j<Max_Clients;j++){
			cli_table[i].user_pipe_fd[j][0] = -1;
			cli_table[i].user_pipe_fd[j][1] = -1;
		}
		for(int j=0;j<Max_Clients;j++){
			char FIFO[100];
			sprintf(FIFO,"user_pipe/%d-%d",i,j);
			unlink(FIFO);
		}
	}
	shmdt(cli_table);
	shmctl(shm_id,IPC_RMID,NULL);
	exit(0);
}

void skip_space(char p[]){
	int len = strlen(p);
	for(int i=len-1;i>=0;i--){
		if(p[i]==' ')
			p[i] = '\0';
		else break;
	}
}

int shell(int fd) //回傳值代表有幾個指令
{
	cli_table = shmat(shm_id,NULL, 0);
	int STDIN = dup(0);
	int STDOUT = dup(1); //用來recovery
	int next = -1;
	IniTable();
	signal(SIGCHLD, sig_handler);
	signal(SIGUSR1, msg_handler);
    setenv("PATH", "bin:.", 1);
	while(next){
		write(fd,"% ",strlen("% "));
		char s[MAX_CMD_LEN];
		// recv(fd,s, MAX_CMD_LEN,0);
		read(fd,s,MAX_CMD_LEN);
		for(int i=0;i<strlen(s);i++)
			if((int)s[i]==13 || (int)s[i]==10) s[i]='\0';
		if(NoNumPipeFunc(s)){
			PC = (PC + 1) % MaxPipe;
			NoNumPipe = NoNumPipeFunc(s);
			next = LineParser(s, STDIN, STDOUT);
			if(next==1) Synchronization();
		}
		else{
			char p[100][MAX_CMD_LEN];
			for(int i=0;i<100;i++) strcpy(p[i],"");
			int cmd = ReadLine(STDIN,s,p);
			if(!cmd) next = 1;
			for(int i=0;i<cmd;i++){
				PC = (PC + 1) % MaxPipe;
				skip_space(p[i]);
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
	}
}

void Login(int ssock,struct sockaddr_in cli_addr){
	char s[200] = "****************************************\n** Welcome to the information server. **\n****************************************\n";
	write(1,s,strlen(s));
	sprintf(s, "*** User '(no name)' entered from %s:%d. ***\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
	write(1,s,strlen(s));
	Broadcast(s);
}

int main(int argc , char *argv[])
{
	signal(SIGCHLD, sig_handler);
	signal(SIGINT, ipc_handler);
	signal(SIGUSR1, SIG_IGN);
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
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))<0){
		fprintf(stderr,"Can't REUSEADDR.");
		exit(1);
	}
	// setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(int));
	bind(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
	listen(sockfd,30);
	if ((devnull = open("/dev/null", O_RDWR)) == -1) exit(1);
	if((shm_id = shmget((key_t)SEMKEY, Max_Clients * sizeof(Client), 0666 | IPC_CREAT))<0)
		printf("Share Memory Error.");
	cli_table = shmat(shm_id,NULL, 0);
	for(int i=0;i<Max_Clients;i++){
		cli_table[i].cli_fd = 0;
		cli_table[i].id = 0;
		cli_table[i].pid = 0;
		for(int j = 0; j < Max_Clients; j++){
			cli_table[i].user_pipe_fd[j][0] = -1;
			cli_table[i].user_pipe_fd[j][1] = -1;
		}
		strcpy(cli_table[i].name,"");
		strcpy(cli_table[i].mesg_data,"");
	}
	while(1){
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd,(struct sockaddr *)&cli_addr,&clilen);
		for(int i=0;i<Max_Clients;i++)
			if(!cli_table[i].id){
				uid = i+1;
				break;
			}
		cli_table[uid-1].cli_fd = newsockfd;
		cli_table[uid-1].cli_addr = cli_addr;
		cli_table[uid-1].id = uid;
		strcpy(cli_table[uid-1].name,"(no name)");
		childpid = fork();
		if(!childpid){
			dup2(newsockfd,0);
			dup2(newsockfd,1);
			dup2(newsockfd,2);
			close(sockfd);
			Login(newsockfd,cli_addr);
			shell(newsockfd);
			// DisplayCMD(newsockfd);
			close(newsockfd);
			dup2(STDIN,0);
			dup2(STDOUT,1);
			dup2(STDERR,2);
			exit(0);
		}
		else if(childpid>0){
			cli_table[uid-1].pid = childpid;
			close(newsockfd);
		}
	}
	// semctl(sem_id, 0, IPC_RMID,NULL);
	return 0;
}

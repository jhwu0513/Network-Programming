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

#define MaxPipe 1001
#define Process_Number 512
#define MAX_CMD_LEN 15001
#define Max_Clients 30

int SERV_TCP_PORT;

typedef struct Client{
	int cli_fd;
	struct sockaddr_in cli_addr;
	int process[Process_Number]; // 每個Client要有獨立的process list
	int Pindex;
	int PC;
	int id;
	int num_pipe_fd[MaxPipe][2];
	int user_pipe_fd[Max_Clients][2];
	int NoNumPipe;
	char name[100];
	char path[100];
	char env_name[1000][100];
	char env_val[1000][100];
	int env_index;
}Client;

Client cli_table[Max_Clients];


void Broadcast(char s[]){
	for(int i=0;i<Max_Clients;i++)
		if(cli_table[i].cli_fd)
			write(cli_table[i].cli_fd,s,strlen(s));
}

void IniTable(Client *user){
	for (int i = 0; i < MaxPipe; i++){
		user->num_pipe_fd[i][0] = -1;
		user->num_pipe_fd[i][1] = -1;
	}
	for(int i = 0; i < Max_Clients; i++){
		user->user_pipe_fd[i][0] = -1;
		user->user_pipe_fd[i][1] = -1;
	}
	strcpy(user->path,"bin:.");
	for (int i = 0; i < Process_Number; i++) user->process[i] = -1;
}

void Synchronization(Client *user){
    for (int i = 0; i < Process_Number; i++)
        if(user->process[i] != -1)
        {
            waitpid(user->process[i], NULL, 0);
            user->process[i] = -1;
        }
}

int Number_Pipe(char *s){
	if((s[0]!='|' && s[0]!='!') || strlen(s)<2) return 0;
	for(int i=1;i<strlen(s);i++)
		if(!isdigit(s[i])) return 0;
	return 1;
}

int CMD(char **arg, int *fd, int Command_Mode, int STDIN, int STDOUT,Client *user)
{
	int pid;
	/*
		Command_Mode :
			0:代表該行只有1個指令，可能是單一普通指令，也有可能有File Redirection
			1:代表遇到Ordinary Pipe
			2:代表遇到|的Number Pipe
			3:代表遇到!的Number Pipe
	*/
	while ((pid = fork()) < 0) usleep(1000);
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
			fprintf(stderr, "Unknown command: [%s].\r\n", arg[0]);
			exit(EXIT_FAILURE);
		}
		
	}
	else if (pid > 0)
	{
		if(user->NoNumPipe){
			user->Pindex = (user->Pindex + 1) % Process_Number;
			user->process[user->Pindex] = pid;
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
	char line[strlen(command) + 1];
	strcpy(line, command);
	char *word = strtok(line, " \n");
	while (word)
	{
		if(Number_Pipe(word)) return 0;
		word = strtok(NULL, " \n");
	}
	return 1;
}

int ReadLine(int fd,char *s,char p[][MAX_CMD_LEN]) //回傳值代表有幾個指令
{
	for(int i=0;i<strlen(s);i++)
		if((int)s[i]==13) s[i]='\0';
    char *a = strtok(s," \r\n");
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
        a = strtok(NULL, " \r\n");
    }
	return cmd;
}

int Input_from_User_Pipe(int fd[],Client *user,int id,char command[]){
	char s[500];
	if(id<=0 || id>30 || !cli_table[id-1].cli_fd){
		sprintf(s, "*** Error: user #%d does not exist yet. ***\n", id);
		Broadcast(s);
		return 1;
	}
	if(user->user_pipe_fd[id-1][0] < 0)
	{
		sprintf(s, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", id, user->id);
		write(user->cli_fd,s,strlen(s));
		return 1;
	}
	else{
		dup2(user->user_pipe_fd[id-1][0], 0);
		close(user->user_pipe_fd[id-1][0]);
		user->user_pipe_fd[id-1][0] = -1;
		user->user_pipe_fd[id-1][1] = -1;
		sprintf(s, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", user->name, user->id, cli_table[id-1].name, id, command);
		Broadcast(s);
	}
	return 0;
}

int LineParser(char Line[], int STDIN, int STDOUT,Client *user)
{
	char cp[strlen(Line) + 1];
	strcpy(cp, Line);
	char **arg = malloc(50 * sizeof(char *));
	memset(arg,'\0',50);
	if (user->num_pipe_fd[user->PC][0] != -1)
	{
		dup2(user->num_pipe_fd[user->PC][0], 0);
		close(user->num_pipe_fd[user->PC][0]);
		close(user->num_pipe_fd[user->PC][1]);
		user->num_pipe_fd[user->PC][0] = -1;
		user->num_pipe_fd[user->PC][1] = -1;
	}
	int devnull;
	if ((devnull = open("/dev/null", O_RDWR)) == -1) exit(1);
	int cnt = 0; 
	char *word = strtok(cp, " \r\n");
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
		else if(!strcmp(word, "name"))
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
				int flag = Input_from_User_Pipe(user->user_pipe_fd[atoi(&input[1])-1],user,atoi(&input[1]),Line); // 從這裡獲取Input，並且把0給dup掉
				if(flag) dup2(devnull,0); // 如果沒有東西可以當作輸入端
			}
			if(target<=0 || target>30 || !cli_table[target-1].cli_fd){
				sprintf(s, "*** Error: user #%d does not exist yet. ***\n", target);
				write(user->cli_fd,s,strlen(s));
				dup2(devnull, 1);
				break;
			}
			if(cli_table[target-1].user_pipe_fd[user->id-1][1] >= 0)
			{
				sprintf(s, "*** Error: the pipe #%d->#%d already exists. ***\n", user->id, target);
				write(user->cli_fd,s,strlen(s));
				dup2(devnull, 1);
			}
			else{
				pipe(cli_table[target-1].user_pipe_fd[user->id-1]);
				dup2(cli_table[target-1].user_pipe_fd[user->id-1][1], 1); // 可能會造成大問題，要把1復原成cli_fd
				close(cli_table[target-1].user_pipe_fd[user->id-1][1]);
				sprintf(s, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", user->name, user->id, Line, cli_table[target-1].name, target);
				Broadcast(s);
			}
			break; //可能會有 >2 <1這種case，所以執行完就要break
		}
		else if(word[0] == '<' && strlen(word) > 1){
			int flag = Input_from_User_Pipe(user->user_pipe_fd[atoi(&word[1])-1],user,atoi(&word[1]),Line);
			if(flag) dup2(devnull,0);
		}
		else if (!strcmp(word,"|")) //如果看到Ordinary Pipe
		{
			int fd[2];
			pipe(fd);
			CMD(arg, fd, 1, STDIN, STDOUT,user);
			memset(arg,'\0',50);
			cnt = 0;
		}
		else if (!strcmp(word, ">"))
		{
			word = strtok(NULL, " \r\n");
			int file;
			file = open(word,O_RDWR | O_CREAT | O_TRUNC ,S_IRWXU | S_IRWXG | S_IRWXO,0777); // 可以改改看
			dup2(file, 1);
			CMD(arg, NULL, 0, STDIN, STDOUT,user);
			dup2(STDOUT, 1);
			return 1;
		}
		else if(!strcmp(word, ">>")){
			word = strtok(NULL, " \r\n");
			int file;
			file = open(word,O_RDWR | O_CREAT | O_APPEND);
			dup2(file, 1);
			CMD(arg, NULL, 0, STDIN, STDOUT,user);
			dup2(STDOUT, 1);
			return 1;
		}
		else if(Number_Pipe(word))  // 他是number pipe
		{
			int Pipe_N = atoi(&word[1]);
			int addr = (user->PC + Pipe_N) % MaxPipe;
			if (user->num_pipe_fd[addr][1] == -1) pipe(user->num_pipe_fd[addr]);
			if (word[0] == '!') CMD(arg, user->num_pipe_fd[addr], 3, STDIN, STDOUT,user);
			else CMD(arg, user->num_pipe_fd[addr], 2, STDIN, STDOUT,user);
            usleep(1000);
			return 2;
		}
		else arg[cnt++] = word;
		word = strtok(NULL, " \r\n");
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
				if(user->id == cli_table[i].id){
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
				write(user->cli_fd,s,strlen(s));
				break;
			}
		}
		if(i==Max_Clients){
			strcpy(user->name,arg[1]);
			sprintf(s, "*** User from %s:%d is named '%s'. ***\n", inet_ntoa(user->cli_addr.sin_addr), ntohs(user->cli_addr.sin_port),user->name);
			for(int i=0;i<Max_Clients;i++)
				if(cli_table[i].cli_fd!=0)
					write(cli_table[i].cli_fd,s,strlen(s));
		}
		free(arg);
		return 1;
	}
	else if(!strcmp(arg[0],"yell")){
		char s[1500];
		sprintf(s, "*** %s yelled ***: %s\n", user->name, arg[1]);
		for(int i=0;i<Max_Clients;i++)
			if(cli_table[i].cli_fd!=0)
				write(cli_table[i].cli_fd,s,strlen(s));
		free(arg);
		return 1;
	}
	else if(!strcmp(arg[0],"tell")){
		char s[1500];
		int flag = 1;
		for(int i=0;i<Max_Clients;i++)
			if(cli_table[i].id==atoi(arg[1]) && cli_table[i].cli_fd!=0){
				sprintf(s, "*** %s told you ***: %s\n", user->name, arg[2]);
				write(cli_table[i].cli_fd,s,strlen(s));
				flag = 0;
			}
		if(flag){
			sprintf(s, "*** Error: user #%s does not exist yet. ***\n", arg[1]);
			write(user->cli_fd,s,strlen(s));
		}
		free(arg);
		return 1;
	}
	else if(!strcmp(arg[0],"printenv")){
		if(!strcmp(arg[1],"PATH")){
			write(STDOUT,user->path,strlen(user->path));
			write(STDOUT,"\r\n",strlen("\r\n"));
		}
		else{
			for(int i=0;i<user->env_index;i++)
				if(!strcmp(user->env_name[i],arg[1])){
					write(STDOUT,user->env_val[i],strlen(user->env_val[i]));
					write(STDOUT,"\r\n",strlen("\r\n"));
				}
		}
		free(arg);
		return 1;
	}
	else if(!strcmp(arg[0],"setenv")){
		if(!strcmp(arg[1],"PATH"))
			strcpy(user->path,arg[2]);
		else{
			strcpy(user->env_name[user->env_index],arg[1]);
			strcpy(user->env_val[user->env_index],arg[2]);
			user->env_index++;
		}
		free(arg);
		return 1;
	}
	else if(!strcmp(arg[0],"exit")){
		free(arg);
		return 0;
	}
	/*
	這裡是為了執行Ordinary Pipe最後的指令
	*/
	CMD(arg,NULL,0,STDIN, STDOUT,user);
	return 1;
}

int single_shell(Client *user,int STDIN,int STDOUT){
	setenv("PATH", user->path, 1);
	char input[MAX_CMD_LEN]="";
	recv(user->cli_fd,input, MAX_CMD_LEN,0);
	for(int i=0;i<strlen(input);i++)
		if((int)input[i]==13 || (int)input[i]==10) input[i]='\0';
	if(NoNumPipeFunc(input)){
		user->PC = (user->PC + 1) % MaxPipe;
		user->NoNumPipe = NoNumPipeFunc(input);
		int syn = LineParser(input, STDIN, STDOUT,user);
		if(!syn) return 1; 
		if(syn==1) Synchronization(user);
	}
	else{
		char p[100][MAX_CMD_LEN];
		for(int i=0;i<100;i++) p[i][0]='\0';
		int cmd = ReadLine(user->cli_fd,input,p);
		for(int i=0;i<cmd;i++){
			user->PC = (user->PC + 1) % MaxPipe;
			user->NoNumPipe = NoNumPipeFunc(p[i]);
			int syn = LineParser(p[i], STDIN, STDOUT,user);
			if(!syn) return 1; //表示exit
			if(syn==1) Synchronization(user);
		}
	}
	return 0; //表示沒有exit
}

void Login(int ssock,struct sockaddr_in cli_addr){
	char s[200] = "****************************************\n** Welcome to the information server. **\n****************************************\n";
	write(ssock,s,strlen(s));
	sprintf(s, "*** User '(no name)' entered from %s:%d. ***\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
	for(int i=0;i<Max_Clients;i++)
		if(cli_table[i].cli_fd!=0)
			write(cli_table[i].cli_fd,s,strlen(s));
	write(ssock,"% ",strlen("% "));
}

void sig_handler(int signo)
{
	int status;
	wait(&status);
}

int main(int argc , char *argv[])
{
	// signal(SIGCHLD, sig_handler);
	fd_set rfds;
	fd_set afds;
	int STDIN = dup(0),STDOUT = dup(1),STDERR = dup(2);
	SERV_TCP_PORT = atoi(argv[1]); //第一個參數假設是port
	int clilen,nfds,msock,exit_index=-1;
	struct sockaddr_in cli_addr,serv_addr;
	msock = socket(AF_INET,SOCK_STREAM,0);
	bzero((char *)&serv_addr,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); //任何網卡位址都可以連
	serv_addr.sin_port = htons(SERV_TCP_PORT);
	int optval;
	setsockopt(msock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	bind(msock,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
	listen(msock,Max_Clients);
	nfds = getdtablesize();
	FD_ZERO(&afds);
	FD_SET(msock,&afds);
	for(int i=0;i<Max_Clients;i++){
		cli_table[i].cli_fd = 0;
		cli_table[i].id = 0;
		strcpy(cli_table[i].name,"");
	}
	while(1){
		memcpy(&rfds,&afds,sizeof(rfds));
		select(nfds,&rfds,(fd_set *)0,(fd_set *)0,(struct timeval *)0);
		if(FD_ISSET(msock,&rfds)){
			int ssock;
			clilen = sizeof(cli_addr);
			ssock = accept(msock,(struct sockaddr *)&cli_addr,&clilen);
			FD_SET(ssock,&afds);
			for(int i=0;i<Max_Clients;i++)
				if(cli_table[i].cli_fd==0){
					cli_table[i].cli_fd = ssock;
					cli_table[i].cli_addr = cli_addr;
					cli_table[i].Pindex = -1; 
					cli_table[i].PC = -1;
					cli_table[i].id = i+1;
					cli_table[i].env_index = 0;
					for(int j=0;j<1000;j++){
						strcpy(cli_table[i].env_name[j],"");
						strcpy(cli_table[i].env_val[j],"");
					}
					IniTable(&cli_table[i]);
					strcpy(cli_table[i].name,"(no name)");
					break;
				}
			Login(ssock,cli_addr);
			exit_index = -1;
		}
		for(int i=0;i<Max_Clients;i++)
			if(FD_ISSET(cli_table[i].cli_fd,&rfds)){
				dup2(cli_table[i].cli_fd,1);
				dup2(cli_table[i].cli_fd,2);
				if(single_shell(&cli_table[i],STDIN,cli_table[i].cli_fd)) exit_index = i;
				else exit_index = -1;
				write(cli_table[i].cli_fd,"% ",strlen("% "));
				dup2(STDOUT,1);
				dup2(STDERR,2);
			}
		if(exit_index>=0){
			FD_CLR(cli_table[exit_index].cli_fd,&afds);
			close(cli_table[exit_index].cli_fd);
			cli_table[exit_index].cli_fd = 0;
			cli_table[exit_index].Pindex = -1;
			cli_table[exit_index].PC = -1;
			cli_table[exit_index].id = 0;
			char s[200];
			for (int i = 0; i < MaxPipe; i++){
				cli_table[exit_index].num_pipe_fd[i][0] = -1;
				cli_table[exit_index].num_pipe_fd[i][1] = -1;
			}
			for(int i = 0; i < Max_Clients; i++){
				cli_table[exit_index].user_pipe_fd[i][0] = -1;
				cli_table[exit_index].user_pipe_fd[i][1] = -1;
			}
			for(int j=0;j<cli_table[exit_index].env_index;j++){
				strcpy(cli_table[exit_index].env_name[j],"");
				strcpy(cli_table[exit_index].env_val[j],"");
			}
			cli_table[exit_index].env_index = 0;
			sprintf(s, "*** User '%s' left. ***\n", cli_table[exit_index].name);
			for(int i=0;i<Max_Clients;i++)
				if(cli_table[i].cli_fd!=0)
					write(cli_table[i].cli_fd,s,strlen(s));
			strcpy(cli_table[exit_index].name,"");
		}
	}
	return 0;
}

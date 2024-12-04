#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {"cp","touch","mkdir","ls","pwd","cat","grep","chmod","diff","cd","exit","help","sendmsg"};

struct message {
	char source[50];
	char target[50]; 
	char msg[200];
};

void terminate(int sig) {
        printf("Exiting....\n");
        fflush(stdout);
        exit(0);
}

void sendmsg(char *user, char *target, char *msg) {
  struct message msgStruct;

  strncpy(msgStruct.source, user, sizeof(msgStruct.source) - 1);
  msgStruct.source[sizeof(msgStruct.source) - 1] = '\0';

  strncpy(msgStruct.target, target, sizeof(msgStruct.target) - 1);
  msgStruct.target[sizeof(msgStruct.target) - 1] = '\0';

  strncpy(msgStruct.msg, msg, sizeof(msgStruct.msg) - 1);
  msgStruct.msg[sizeof(msgStruct.msg) - 1] = '\0';

  int serverFifo = open("serverFIFO", O_WRONLY);
  if(serverFifo == -1) {
    perror("Failed to open server FIFO");
    return;
  }

  if(write(serverFifo, &msgStruct, sizeof(msgStruct)) == -1) {
    perror("Failed to write to server FIFO");
  }
  
  close(serverFifo);
}

void* messageListener(void *arg) {
    char fifoName[20];
    snprintf(fifoName, sizeof(fifoName), "%s", uName);

    int userFifo = open(fifoName, O_RDONLY);
    if (userFifo == -1) {
      perror("Failed to open user FIFO");
      pthread_exit((void*)1);
    }

    struct message incomingMsg;

    while(1) {
      ssize_t bytesRead = read(userFifo, &incomingMsg, sizeof(incomingMsg));
      if (bytesRead == -1) {
        perror("Failed to read from user FIFO");
        break;
      }

      if(bytesRead == sizeof(incomingMsg)) {
        printf("Incoming message from %s: %s\n", incomingMsg.source, incomingMsg.msg);
      }
    }

  close(userFifo);
  pthread_exit((void*)0);
}


int isAllowed(const char*cmd) {
	int i;
	for (i=0;i<N;i++) {
		if (strcmp(cmd,allowed[i])==0) {
			return 1;
		}
	}
	return 0;
}

int main(int argc, char **argv) {
    pid_t pid;
    char **cargv; 
    char *path;
    char line[256];
    int status;
    posix_spawnattr_t attr;

    if (argc!=2) {
	printf("Usage: ./rsh <username>\n");
	exit(1);
    }
    signal(SIGINT,terminate);

    strcpy(uName,argv[1]);

    pthread_t listenerThread;
    if (pthread_create(&listenerThread, NULL, messageListener, NULL) != 0) {
      perror("Failed to create message listener thread");
      exit(EXIT_FAILURE);
    }

    while (1) {

	    fprintf(stderr,"rsh>");

	    if(fgets(line,256,stdin)==NULL) continue;

	    if(strcmp(line,"\n")==0) continue;

	    line[strlen(line)-1]='\0';

	    char cmd[256];
	    char line2[256];
	    strcpy(line2,line);
	    strcpy(cmd,strtok(line," "));

	    if(!isAllowed(cmd)) {
	    	printf("NOT ALLOWED!\n");
	    	continue;
	    }

      if(strcmp(cmd, "sendmsg") == 0) {
        char *target = strtok(NULL, " ");
        if (target == NULL) {
          printf("sendmsg: you have to specify target user\n");
          continue;
        }
      
        char *message = strtok(NULL, "");
        if (message == NULL) {
          printf("sendmsg: you have to enter a message\n");
          continue;
        }
      
        sendmsg(uName, target, message);
        continue;
      }

	    if(strcmp(cmd,"exit")==0) break;

	    if(strcmp(cmd,"cd")==0) {
	    	char *targetDir=strtok(NULL," ");
	    	if(strtok(NULL," ")!=NULL) {
	    		printf("-rsh: cd: too many arguments\n");
	    	}
	    	else {
	    		chdir(targetDir);
	    	}
	    	continue;
	    }

	    if(strcmp(cmd,"help")==0) {
	    	printf("The allowed commands are:\n");
	    	for (int i=0;i<N;i++) {
	    		printf("%d: %s\n",i+1,allowed[i]);
	    	}
	    	continue;
	    }

	    cargv = (char**)malloc(sizeof(char*));
	    cargv[0] = (char *)malloc(strlen(cmd)+1);
	    path = (char *)malloc(9+strlen(cmd)+1);
	    strcpy(path,cmd);
	    strcpy(cargv[0],cmd);

	    char *attrToken = strtok(line2," "); /* skip cargv[0] which is completed already */
	    attrToken = strtok(NULL, " ");
	    int n = 1;
	    while (attrToken!=NULL) {
	    	n++;
	    	cargv = (char**)realloc(cargv,sizeof(char*)*n);
	    	cargv[n-1] = (char *)malloc(strlen(attrToken)+1);
	    	strcpy(cargv[n-1],attrToken);
	    	attrToken = strtok(NULL, " ");
	    }
	    cargv = (char**)realloc(cargv,sizeof(char*)*(n+1));
	    cargv[n] = NULL;

	    // Initialize spawn attributes
	    posix_spawnattr_init(&attr);

	    // Spawn a new process
	    if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0) {
	    	perror("spawn failed");
	    	exit(EXIT_FAILURE);
	    }

	    // Wait for the spawned process to terminate
	    if (waitpid(pid, &status, 0) == -1) {
	    	perror("waitpid failed");
	    	exit(EXIT_FAILURE);
	    }

	    // Destroy spawn attributes
	    posix_spawnattr_destroy(&attr);

    }
    return 0;
}

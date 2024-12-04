#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

struct message {
    char source[50];
    char target[50]; 
    char msg[200]; // message body
};

void terminate(int sig) {
    printf("Exiting...\n");
    fflush(stdout);
    exit(0);
}

int main() {
  int target;
  int server;
  int dummyfd;

  struct message req;

  signal(SIGPIPE, SIG_IGN);  // Ignore broken pipe signals

  // Open the server FIFO for reading and a dummy write to keep it open
  server = open("serverFIFO", O_RDONLY);
  dummyfd = open("serverFIFO", O_WRONLY);

  while (1) {
      // Read requests from serverFIFO
      if(read(server, &req, sizeof(struct message)) != sizeof(struct message)) continue;
      printf("Received a request from %s to send the message %s to %s.\n", req.source, req.msg, req.target);

      // Open target FIFO and write the whole message struct to the target FIFO
      target = open(req.target, O_WRONLY);
       
      write(target, &req, sizeof(struct message));
      close(target); // Close target FIFO after writing
  }
  close(server);
  return 0;
}


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_STRING_LENGTH 100
#define HELP 0
#define REGISTER 1
#define DEREGISTER 2
#define SHOW_USER 3
#define SEND_MSG 4
#define QUIT 5

int apriConnessioneTCP(int, char*);
int inviaTCPInt(int, int);
int inviaTCPStringa(int, char*);
int riceviTCPInt(int, int*);
int riceviTCPStringa(int, char*);
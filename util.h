#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#define MAX_DEGREE 10
#define MAX_NUM_NODES 24
#define DEBUG 1
#define TIMEOUT 20

/*
 * Link struct
 * Fields for link dstinations, port and socket descriptor and cost
 */
typedef struct {
    char dstA[10];
    int dstA_port;
    char dstB[10];
    int dstB_port;
    int cost;
    int tmp_fd;     // socket for connect or listen
    int connect_fd; // socket for connection
    int connected;
    struct sockaddr_in local_addr, remote_addr;
} Link;

/*
 * Router struct
 * Fields for Router ID, Link array and number of links
 */

typedef struct {
    char ID[10];
    Link links[MAX_DEGREE];
    int link_cnt;
} Router;

/*
 * Link State Packet struct
 * Fields for Router ID, neighbors and link costs
 */

typedef struct {
    char ID[10];
    int Seq;
} LSP;


/*
 * void config_router_link(Router *router, char *fline)
 * inputs:  Router *router: pointer to router to be configured
 *          fline *line: line read from config file
 * Initialize router links with fline.
 */

void config_router_link(Router *router, char *fline)
{
    char *delimiters = "<,>";
    char *pch = NULL;
    Link *link;
    // parse line read from file
    pch = strtok(fline, delimiters);
    if (pch != NULL)
    {
        // identify router
        if (strcmp(pch, router->ID) == 0)
        {
            link = &(router->links[router->link_cnt]);
            // config dstA_port
            strcpy(link->dstA, pch);
            // config dstA_port
            pch = strtok(NULL, delimiters);
            link->dstA_port = atoi(pch);
            // config dstB
            pch = strtok(NULL, delimiters);
            strcpy(link->dstB, pch);
            // config dstB_port
            pch = strtok(NULL, delimiters);
            link->dstB_port = atoi(pch);
            // config link cost
            pch = strtok(NULL, delimiters);
            link->cost = atoi(pch);
            // increment link_cnt
            router->link_cnt++;
        }
    }
}

/*
 * void print_router(Router *router)
 * input: Router *router: pointer to router.
 * print router_id, number of neighbors and links
 */
void print_router(Router *router)
{
    printf("Router ID: %s, num of links %d\n", router->ID, router->link_cnt);
    int i;
    Link *link;
    for (i=0; i<router->link_cnt; i++)
    {
        link = &(router->links[i]);
        printf("%s,%d,%s,%d,%d\n", link->dstA, link->dstA_port, link->dstB, link->dstB_port, link->cost);
    }
}

/*
 * int timeout_recvfrom
 * inputs:  int sock: socket descriptor
 *          void *buf: buffer to receive msg
 *          int length: length of buffer
 *          struct sockaddr* connection: address
 * outpus:  return 1 for successfully received
 *          return 0 for timeout
 */
int timeout_recvfrom (int sock, void *buf, int length, struct sockaddr *connection)
{
    // printf("in timeout_recvfrom\n");
    fd_set socks;
    // add socket into fd_set
    FD_ZERO(&socks);
    FD_SET(sock, &socks);
    struct timeval t;
    t.tv_usec = TIMEOUT;
    int connection_len = sizeof(connection);

    if (select(sock + 1, &socks, NULL, NULL, &t) &&
            recvfrom(sock, buf, length, 0, (struct sockaddr *)connection, &connection_len)>=0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/* 
 * int timeout_accept
 * inputs:  
 *          
 * output:  return 1 for successfully accept connection
 *          return 0 for timeout
 */



/* 
 * int timeout_connect
 * inputs:  
 *          
 * outpus:  return 1 for successfully accept connection
 *          return 0 for timeout
 *
 */




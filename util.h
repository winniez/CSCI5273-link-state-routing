#include<stdio.h>
#include<stdlib.h>
#include<string.h>


#define MAX_DEGREE 10
#define MAX_NUM_NODES 24
typedef struct {
    char dstA[10];
    int dstA_port;
    char dstB[10];
    int dstB_port;
    int cost;
} Link;

typedef struct {
    char ID[10];
    Link links[MAX_DEGREE];
    int link_cnt;
} Router;

void config_router_link(Router* router, char* fline)
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
            printf("match %s\n", router->ID);
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




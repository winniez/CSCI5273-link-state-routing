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
#define LSP_INTERVAL 5
#define LSP_TTL 10  // forwarding hop count, decrement when forwarding
#define MAX_COST 10000
typedef struct{
    char dst[10];
    int cost;
} LinkStateEntry;

/*
 * Link State Packet struct
 * Fields for Router ID, neighbors and link costs
 */

typedef struct {
    char ID[10];
    int seq;
    int TTL;
    int len;
    LinkStateEntry table[MAX_DEGREE];
} LSP;

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
 * RoutingTableEntry
 * Fields for dstination, cost, outgoing port and destination port 
 */
typedef struct {
    char dst[10];
    int cost;
    int out_port;
    int dst_port;
} RoutingTableEntry;

/* 
 * RoutingTable
 * Fields for routing table, length
 */
typedef struct {
    RoutingTableEntry tableContent[MAX_NUM_NODES];
    int len;
} RoutingTable;

/*
 * LSParchive
 * Fields for lsp archiving, length
 */
typedef struct {
    LSP archive[MAX_DEGREE];
    int len;
} LSParchive;


/*
 * Router struct
 * Fields for Router ID, Link array and number of links
 */

typedef struct {
    char ID[10];
    Link links[MAX_DEGREE];
    int link_cnt;
    time_t send_time;  // timestamp of latest LSP sent out
    LSP self_lsp;
    int self_lsp_seq;
    RoutingTable routingtable;
    LSParchive lsparchive;
} Router;


/* 
 * Initialize routing table
 */
void Init_Routing_Table(Router *router)
{
    int i;
    Link *link;
    RoutingTableEntry *entry;
    router->routingtable.len = router->link_cnt;
    for (i=0; i<router->link_cnt; i++)
    {
        link = &(router->links[i]);
        entry = &(router->routingtable.tableContent[i]);
        strcpy(entry->dst, link->dstB);
        entry->cost = link->cost;
        entry->out_port = link->dstA_port;
        entry->dst_port = link->dstB_port;
    }    
}    


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
 * void print_router_neighbors(Router *router)
 * input: Router *router: pointer to router.
 * print router_id, number of neighbors and links
 */
void print_router_neighbors(Router *router)
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
 * void print_router_routing_table(Router *router)
 * intput:  Router *router: pointer to router
 *          FILE *file: file to write in
 * print routing table
 *
 */
void print_router_routing_table(Router *router, FILE* file)
{
    int i;
    char str[1024];
    sprintf(str, "Dstination\tCost\tOut Port\tDstination Port\n");
    if (file)
    {
        fwrite(str, sizeof(char), strlen(str), file);
    }
    if (DEBUG)
    {
        printf("%s", str);
    }    
    for (i=0; i<router->routingtable.len; i++)
    {
        sprintf(str, "%s\t\t%d\t\t%d\t\t%d\n",
                router->routingtable.tableContent[i].dst,
                router->routingtable.tableContent[i].cost,
                router->routingtable.tableContent[i].out_port,
                router->routingtable.tableContent[i].dst_port);
        if (file)
        {
            fwrite(str, sizeof(char), strlen(str), file); 
        }
        if(DEBUG)
        {
            printf("%s", str);
        }
    }
}

/*
 *
 */
void print_lsp(LSP *lsp, FILE *file)
{
    int i;
    char str[2048];
    char tmp[24];
    sprintf(str, "Neighbors of %s\t", lsp->ID);
    for (i=0; i<lsp->len; i++)
    {
        strcat(str, lsp->table[i].dst);
        strcat(str, "\t");
    }
    strcat(str, "\n");
    if(DEBUG) printf("%s", str);
    if (file) fwrite(str, sizeof(char), strlen(str), file);
    sprintf(str, "Link Cost\t");
    for (i = 0; i<lsp->len; i++)
    {
        sprintf(tmp, "%d\t", lsp->table[i].cost);
        strcat(str, tmp);
    }
    strcat(str, "\n");
    if (DEBUG) printf("%s", str);
    if (file) fwrite(str, sizeof(char), strlen(str), file);

}


/* 
 * Update router archive with recvd lsp
 * int update_LSP_database(Router *router, LSP *lsp)
 * inputs:  Router *router
 *          LSP *lsp, pointer to recvd lsp
 * return:  0 for topology unchanged and router have seen this lsp before
 *          1 for topology unchanged and router haven't seen this lsp and need forwarding to other ports
 *          2 for changed topology and need forwarding
 */
int update_LSP_database(Router *router, LSP *lsp)
{
    // initialize return value
    int rtn = 0;
    int j;
    int exist = 0;
    // check TTL, discard lsp if TTL = 0
    if (lsp->TTL <=0)
    {
        rtn = 0;
        return rtn;
    }    
    
    for (j=0; j<router->lsparchive.len; j++)
    {
        // identify lsp sender
        if(strcmp(lsp->ID, router->lsparchive.archive[j].ID)==0)
        {
            // compare seq
            if (lsp->seq > router->lsparchive.archive[j].seq)
            {
                // newer lsp, archive and forward
                router->lsparchive.archive[j].TTL = lsp->TTL;
                router->lsparchive.archive[j].seq = lsp->seq;
                rtn = 1;
                // scan table to determine if neighbor changes
                LSP *tmplsp = &(router->lsparchive.archive[j]);
                int k;
                for (k=0; k<lsp->len; k++)
                {
                    if (strcmp(lsp->table[k].dst, tmplsp->table[k].dst)!=0)
                    {
                        rtn = 2;
                        strcpy(tmplsp->table[k].dst, lsp->table[k].dst);
                    }    
                    if (lsp->table[k].cost != tmplsp->table[k].cost)
                    {
                        rtn = 2;
                        tmplsp->table[k].cost = lsp->table[k].cost;
                    }    
                }    

                if (tmplsp->len != lsp->len)
                {
                    // topology changed, update archive lsp and break
                    tmplsp->len = lsp->len;
                    rtn = 2;
                }
            }
            // set exist
            exist = 1;
            // otherwise, have received this lsp before, discard this lsp   
        }    
    }
    // add new lsp not in archive
    if (!exist)
    {
        // copy lsp
        LSP *tmplsp = &(router->lsparchive.archive[router->lsparchive.len]);
        strcpy(tmplsp->ID, lsp->ID);
        tmplsp->TTL = lsp->TTL;
        tmplsp->len = lsp->len;
        tmplsp->seq = lsp->seq;
        int k;
        for (k=0; k<lsp->len; k++)
        {
            strcpy(tmplsp->table[k].dst, lsp->table[k].dst);
            tmplsp->table[k].cost = lsp->table[k].cost;
        }    
        router->lsparchive.len++;
        rtn = 2;
    }    
    return rtn;
}

/* 
 * compute Dijastra's shortest path tree
 * inputs:  RouteingTable *routingrable, pointer to router's routing table
 *          LSP *lsp, lsp used to update routing table
 *          int mode, 1 for add lsp, 0 for remove lsp
 * output:  1 for updated routing table
 *          0 for routing table unchanged
 */
int update_routing_table(Router *router, LSP *lsp, int mode)
{
    int rtn = 0;
    int i, j, k,newcost, basic_cost = MAX_COST;
    int basic_out_port, basic_dst_port;
    int exist = 0;
    RoutingTable *routingtable = &(router->routingtable);
    if (mode)
    {// add lsp mode
        // find cost between(router->ID, lsp->ID) in current routing table
        for (i=0; i<routingtable->len; i++)
        {
            if(strcmp(lsp->ID, routingtable->tableContent[i].dst)==0)
            {
                basic_cost = routingtable->tableContent[i].cost;
                basic_out_port = routingtable->tableContent[i].out_port; 
                basic_dst_port = routingtable->tableContent[i].dst_port;    
            }    
        }    

        for (i=0; i<lsp->len; i++)
        {
            exist = 0;
            for (j=0; j<routingtable->len; j++)
            {
                if(strcmp(lsp->table[i].dst, routingtable->tableContent[j].dst)==0)
                {
                    // destination already in routing table
                    exist = 1;
                    // compare cost
                    newcost = basic_cost + lsp->table[i].cost;
                    if (newcost < routingtable->tableContent[j].cost)
                    {// find path with lower cost
                        rtn = 1;
                        routingtable->tableContent[j].cost = newcost;
                        routingtable->tableContent[j].out_port = basic_out_port;
                        routingtable->tableContent[j].dst_port = basic_dst_port;
                    }    
                }
                if (strcmp(lsp->table[i].dst, router->ID)==0)
                {
                    exist = 1;
                }    
            }
            if (!exist)
            {// add dst into routing table
                strcpy(routingtable->tableContent[routingtable->len].dst, lsp->table[i].dst);
                newcost = basic_cost + lsp->table[i].cost;
                routingtable->tableContent[routingtable->len].cost = newcost;
                routingtable->tableContent[routingtable->len].out_port = basic_out_port;
                routingtable->tableContent[routingtable->len].dst_port = basic_dst_port;
                routingtable->len++;
                rtn = 1;
            }    
        }
    }
    else
    {// remove lsp info from routing table
    }    
    return rtn;
}    



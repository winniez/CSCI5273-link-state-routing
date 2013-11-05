#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<netinet/in.h>
#include<netdb.h>
#include<unistd.h>
#include"util.h"

#define DEBUG 1

/*
 * ./routed_LS <RouterID> <LogFileName> <Initialization file>
 *  argv[0]     argv[1]     argv[2]         argv[3]
 */

int main(int argc, char *argv[]){

    FILE *config_file;
    FILE *log_file;
    
    // router id
    char router_id[10];
    strcpy(router_id, argv[1]);
    int rd_buffer_len = 128;
    int rd_line_len;
    char line[rd_buffer_len];
    char *ptrline = &line[0];

    // declare and initialize router
    Router router;
    strcpy(router.ID, argv[1]);
    router.link_cnt = 0;

    // open configuration file
    config_file = fopen(argv[3], "r");
    if (!config_file)
    {
    printf("%s: Failed open configuration file %s \n", argv[0], argv[1]);
    exit(1);
    }
    // read config file
    do{
        rd_line_len = getline(&ptrline, &rd_buffer_len, config_file);
        // printf("%s", line);
        // take the line read from file to initialize route
        config_router_link(&router, line);

    }while(rd_line_len != -1);
    fclose(config_file);
    // print router for debug
    print_router(&router);


    
    
    return 0;

}

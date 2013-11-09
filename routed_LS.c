#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<sys/fcntl.h>
#include<netinet/in.h>
#include<netdb.h>
#include<unistd.h>
#include <time.h>
#include"util.h"


/*
 * ./routed_LS <RouterID> <LogFileName> <Initialization file>
 *  argv[0]     argv[1]     argv[2]         argv[3]
 */

int main(int argc, char *argv[]){

    FILE *config_file;
    FILE *log_file;

    static const char ip_addr[] = "127.0.1.1";
    
    // router id
    char router_id[10];
    strcpy(router_id, argv[1]);
    int rd_buffer_len = 128;
    int rd_line_len = 0;
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
    rd_line_len = getline(&ptrline, &rd_buffer_len, config_file);
    while(rd_line_len != -1)
    {
        // take the line read from file to initialize route
        config_router_link(&router, line);
        rd_line_len = getline(&ptrline, &rd_buffer_len, config_file);

    }
    // close configuration file
    fclose(config_file);
    
    // print router for debug
    print_router(&router);

    // open log file
    log_file = fopen(argv[2], "w");
    if (!log_file)
    {
        printf("Failed open log file %s for router %s.\n", argv[2], argv[1]);
        exit(1);
    }


    // create socket and set non-blocking mode
    int i =0;
    for (i=0; i<router.link_cnt; i++)
    {
        if ((router.links[i].tmp_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            printf("%s, %d, %s, %d: cannot open socket.\n", 
                    router.links[i].dstA, router.links[i].dstA_port, 
                    router.links[i].dstB, router.links[i].dstB_port);
        }
        // remove the non-block setting here
        // if connect is unblock mode, connect would immediately return failure
        // when connect request queuing on listening port
        // Accept is set to nonblock mode and iteratively check all ports
        
        // set socket to non-block mode
        /*
        if (fcntl(router.links[i].tmp_fd, F_SETFL, O_NDELAY)<0)
        {
            printf("%s, %d, %s, %d: cannot set non-block mode socket.\n",
                    router.links[i].dstA, router.links[i].dstA_port,
                    router.links[i].dstB, router.links[i].dstB_port);
        }
        */
        // set address
        bzero(&router.links[i].local_addr, sizeof(router.links[i].local_addr));
        bzero(&router.links[i].remote_addr, sizeof(router.links[i].local_addr));
        router.links[i].local_addr.sin_family = AF_INET;
        router.links[i].remote_addr.sin_family = AF_INET;
        router.links[i].local_addr.sin_addr.s_addr = INADDR_ANY;
        router.links[i].remote_addr.sin_addr.s_addr = INADDR_ANY;
        router.links[i].local_addr.sin_port = htons(router.links[i].dstA_port);
        router.links[i].remote_addr.sin_port = htons(router.links[i].dstB_port);
        // set connection flag
        router.links[i].connected = 0;
    }


    // attempt to connect, if failed create new socket for listening
    for (i=0; i<router.link_cnt; i++)
    {
        if (connect(router.links[i].tmp_fd, (struct sockaddr*)&(router.links[i].remote_addr), 
                    sizeof(router.links[i].remote_addr)) == 0)
        {
            router.links[i].connected = 1;
            router.links[i].connect_fd = router.links[i].tmp_fd;
            printf("CONNECT SUCCESS!: %s, %d, %s, %d\n",
                    router.links[i].dstA, router.links[i].dstA_port,
                    router.links[i].dstB, router.links[i].dstB_port);
        }
        else    // connect failure
        {
            router.links[i].connected = 0;
            printf("CONNECT FAILED!: %s, %d, %s, %d\n",
                    router.links[i].dstA, router.links[i].dstA_port,
                    router.links[i].dstB, router.links[i].dstB_port);
            // create listen socket
            if ((router.links[i].tmp_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                printf("%s, %d, %s, %d: cannot open socket.\n",
                    router.links[i].dstA, router.links[i].dstA_port,
                    router.links[i].dstB, router.links[i].dstB_port);
            }    
            // bind socket with local address
            if (bind(router.links[i].tmp_fd, (struct sockaddr*)&(router.links[i].local_addr), 
                        sizeof(router.links[i].local_addr))<0)
            {
                printf("BIND FAILED!: %s, %d, %s, %d\n",
                        router.links[i].dstA, router.links[i].dstA_port,
                        router.links[i].dstB, router.links[i].dstB_port);
                exit(1);
            }

            // set socket to non-block mode
            if (fcntl(router.links[i].tmp_fd, F_SETFL, O_NDELAY)<0)
            {
                printf("%s, %d, %s, %d: cannot set non-block mode socket.\n",
                        router.links[i].dstA, router.links[i].dstA_port,
                        router.links[i].dstB, router.links[i].dstB_port);
            }
            // listen on port
            if (listen(router.links[i].tmp_fd, MAX_DEGREE)<0)
            {
                printf("LISTEN FAILED!: %s, %d, %s, %d\n",
                        router.links[i].dstA, router.links[i].dstA_port,
                        router.links[i].dstB, router.links[i].dstB_port);
            }
        }
    }

    while(1)
    {
        // scan all ports, accept connection on unconnected ports
        for (i=0; i<router.link_cnt; i++)
        {
            if (!router.links[i].connected)
            {// call accept on unconnected port
                router.links[i].connect_fd = accept(router.links[i].tmp_fd, NULL, sizeof(struct sockaddr_in));
                if (router.links[i].connect_fd < 0)
                {// failed to connect

                }
                else
                {// connection established
                    router.links[i].connected = 1;
                    printf("CONNECTION ESTABLISHED!: %s, %d, %s, %d\n",
                        router.links[i].dstA, router.links[i].dstA_port,
                        router.links[i].dstB, router.links[i].dstB_port);
                }
                
                
            }
            else
            {// connection established, send LSP on port
                
            }    
        }
    }


    return 0;

}

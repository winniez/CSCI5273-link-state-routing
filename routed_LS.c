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
    char tmp_char_buffer[1024];
    int nbytes;
    int lsp_recv_option;
    // router id
    char router_id[10];
    strcpy(router_id, argv[1]);
    size_t rd_buffer_len = 128;
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
    print_router_neighbors(&router);

    // open log file
    log_file = fopen(argv[2], "wb");
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
    // initialize router timer
    time_t cur_time;
    time(&router.send_time);
    // initialize LSP of this router
    strcpy(router.self_lsp.ID, router.ID);
    router.self_lsp_seq = 1;
    router.self_lsp.TTL = LSP_TTL;
    router.self_lsp.seq = router.self_lsp_seq;
    router.self_lsp.len = router.link_cnt;
    for (i=0; i<router.link_cnt; i++)
    {
        strcpy(router.self_lsp.table[i].dst, router.links[i].dstB);
        router.self_lsp.table[i].cost = router.links[i].cost;
    }
    // initialize router archive
    router.lsparchive.len = 0;

    // receive buffer
    LSP buffer_lsp;
    // initialize router routing table
    Init_Routing_Table(&router);
    // print routing table
    if (DEBUG)
    {
         print_router_routing_table(&router, log_file);
    }

    // prompt
    printf("Input exit to exit router.\n");
    while(1)
    {
        // scan all ports, accept connection on unconnected ports
        for (i=0; i<router.link_cnt; i++)
        {
            if (!router.links[i].connected)
            {// call accept on unconnected port
                router.links[i].connect_fd = accept(router.links[i].tmp_fd, NULL, sizeof(struct sockaddr_in));
                if (router.links[i].connect_fd > 0)
                {// connection established
                    router.links[i].connected = 1;
                    printf("CONNECTION ESTABLISHED!: %s, %d, %s, %d\n",
                            router.links[i].dstA, router.links[i].dstA_port,
                            router.links[i].dstB, router.links[i].dstB_port);
                    // set connect_fd to non-block mode
                    if (fcntl(router.links[i].connect_fd, F_SETFL, O_NDELAY)<0)
                    {
                        printf("%s, %d, %s, %d: cannot set non-block mode socket.\n",
                                router.links[i].dstA, router.links[i].dstA_port,
                                router.links[i].dstB, router.links[i].dstB_port);

                    }
                }
            }
        }
        // check time, send self lsp on all ports with established link
        time(&cur_time);
        if (difftime(cur_time, router.send_time) >= (double)5.0)
        {
            router.send_time = cur_time;
            // update lsp seq
            router.self_lsp.seq++;
            for (i=0; i<router.link_cnt; i++)
            {
                if (router.links[i].connected)
                {
                    nbytes = send(router.links[i].connect_fd, &router.self_lsp, sizeof(LSP), 0);
                    if (nbytes == -1) 
                    {
                        printf("Failed to send on link: %s, %d, %s, %d\n",
                                router.links[i].dstA, router.links[i].dstA_port,
                                router.links[i].dstB, router.links[i].dstB_port);
                    }
                    else
                    {
                        printf("Send LSP to: %s\n", router.links[i].dstB);
                    }
                }
            }
        }

        // receive LSP on all ports with established link
        for (i=0; i<router.link_cnt; i++)
        {
            if (router.links[i].connected)
            {
                nbytes = recv(router.links[i].connect_fd, &buffer_lsp, sizeof(LSP), 0); 
                if (nbytes > 0)
                {   // data received
                    printf("Received LSP with ID %s, seq %d\n", buffer_lsp.ID, buffer_lsp.seq);
                    // store recvd lsp into database and determine if need forwarding
                    // also determine if need update topology and recompute
                    lsp_recv_option = update_LSP_database(&router, &buffer_lsp); 
                    if (lsp_recv_option > 0)
                    {// forward on other connected ports
                        // decrement TTL of forwarding LSP
                        buffer_lsp.TTL--;
                        int j;
                        for (j=0; j<router.link_cnt; j++)
                        {
                            if ((j != i) && (router.links[j].connected))
                            {
                                nbytes = send(router.links[j].connect_fd, &buffer_lsp, sizeof(LSP), 0);
                                if (nbytes == -1)
                                {
                                     printf("Failed to send on link: %s, %d, %s, %d\n",
                                             router.links[j].dstA, router.links[j].dstA_port,
                                             router.links[j].dstB, router.links[j].dstB_port);
                                }
                                else
                                {
                                    printf("Forward LSP from %s to %s\n", buffer_lsp.ID, router.links[j].dstB);
                                }
                            }
                        }
                    }
                    if (lsp_recv_option == 2)
                    {// run dijastra's algorithm
                        printf("recompute routing table...\n");
                        if (update_routing_table(&(router), &buffer_lsp, 1))
                        {
                            time(&cur_time);
                            sprintf(tmp_char_buffer, "UTC:\t%s\n", asctime(gmtime(&cur_time)));
                            printf("%s",tmp_char_buffer);
                            fwrite(tmp_char_buffer, sizeof(char), strlen(tmp_char_buffer), log_file);
                            print_lsp(&buffer_lsp, log_file);
                            print_router_routing_table(&router, log_file);
                        }
                    }
                }
            }

        }

        // check input
        /*
        gets(cmd_input);
        if(!strcmp(cmd_input, "exit"))
        {// exit command from terminal
            printf("Router %s exiting...\n", router.ID);
            // send exit packet 
            break;
        }
        */
    }
    fclose(log_file);
    // close all sockets
    for (i=0; i<router.link_cnt; i++)
    {
        close(router.links[i].connect_fd);
        close(router.links[i].tmp_fd);
    }
    return 0;

}


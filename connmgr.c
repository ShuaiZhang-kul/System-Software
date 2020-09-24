#define _GNU_SOURCE
#include <stdio.h>
#include "errmacros.h"
#include <string.h>
#include "connmgr.h"
#include <pthread.h>


#define PORT 5678
#define MAX_CONN 300  // state the max. number of connections the server will handle before exiting

#ifndef TIMEOUT
  #define TIMEOUT 10
#endif


int epfd=0;
int time_s= (TIMEOUT*1000);
dplist_t * socket_dplist = NULL;

void connmgr_listen(int port_number, sbuffer_t ** buffer,pthread_mutex_t *data_noread_mutex,pthread_mutex_t *FIFO_mutex,int pipe_write_fd)
{
    //Initiate variables then create dplist and server-socket(passive-open)
    int data_noread_mutex_status= 1;    //    0 means trylock success
    tcpsock_t * server, * client;
    sensor_data_t data;               //Used to read data from socket as an intermediate-buffer
    socket_with_sensor *inter;        //intermediate element
    int  request_fd;
    int  client_fd;
    int bytes, result;
    int conn_counter = 0;             //Used to record how many clients exist in order to judge shutting down the server
    int status=0;
    socket_dplist=dpl_create(&con_element_copy,&con_element_free,&con_element_compare);              //Creating a dplist and each node stores one socket_sensor struct including server and client
    printf("Test server is started\n");
    if (tcp_passive_open(&server,port_number)!=TCP_NO_ERROR)       //Create server socket
        {
            printf("tcp_passive_open failed\n");
            exit(EXIT_FAILURE);
        }
    inter=create_element();                             //Create one element
    inter->connection_sock=server;                       //Assign the address of server-socket struct into (inter->connection_sock)
    inter->record.ts=time(NULL);
    dpl_insert_at_index(socket_dplist,inter,dpl_size(socket_dplist),false);        //Insert the first sensor_socket struct which is belong to server into the dplist
   
    
    
    //Now it is time to create Epoll-monitor
    struct epoll_event ev, events[20];  //ev is used to store the sockets it is going to monitor;event is used to receive the active socket at one time(20 means event can receive maximum 20 active socket at one time )
    epfd = epoll_create1(0);  //This function is like making a dplist to store monitor-node of ev and once some nodes are active,they will be copied to EVENT to access the exact sockets
    if(epfd == -1)
        {
            fprintf(stderr, "Failed to create epoll file descriptor\n");
            exit(0);
        }
    tcp_get_sd(server,&request_fd);
    ev.data.ptr=inter;              //Store the address of sensor_socket,because the EVENT can only return the sd of active socket and a void pointer which is set up when this ev-node is create,this can be used to access the                                corresponding sensor of one certain active socket and refresh the sensor data of it for later usage(to decide whether to shut the socket or not)
    ev.data.fd=request_fd;          //Set up which socket is going to be monitored for this node
    ev.events=EPOLLIN;              //Set up which kind of event into socket will be report(information-in event for this node(socket))
    status=epoll_ctl(epfd,EPOLL_CTL_ADD,request_fd,&ev);  //Add this ev node into epoll-"dplist" whose file description is epfd
    if(status==-1)
    {
         fprintf(stderr, "Failed to add epoll server event\n");
    }
    while (1)                        //Endless loop uesd to listen
    {
        socket_with_sensor * current_element=NULL;
        tcpsock_t * current_sock=NULL;
        int event_num=0;                                                 //Used to receive number of active socket at one time
        event_num = epoll_wait(epfd,events,MAX_CONN,time_s);            //if there is no signal coming in within TIMEOUT-s,return 0
        if(event_num==0)
        {
            printf("Waiting time out at first epoll_wait\n");
        }
        if(event_num == -1)
        {
                 fprintf(stderr, "Failed to create epoll file descriptor\n");
                 exit(0);
        }
        
        for(int i=0; i < event_num; ++i)            //Loop through the EVENT array to get the information of each active socket at this time
        {
            //Situation1: New client request
            if(events[i].data.fd == request_fd)     //This socket is server socket and meaning there is a request from a new client
            {
               if (tcp_wait_for_connection(server,&client)!=TCP_NO_ERROR) exit(EXIT_FAILURE);
                printf("Incoming client connection\n");
                conn_counter++;                      //one more client socket
                inter=create_element();                             //The same procedure as putting server socket into epoll monitor-dplist except it is new client socket this time
                inter->connection_sock=client;
                dpl_insert_at_index(socket_dplist,inter,dpl_size(socket_dplist),false);
                tcp_get_sd(client,&client_fd);
                ev.data.fd= client_fd;
                ev.data.ptr=inter;
                ev.events=EPOLLIN;
                status= epoll_ctl(epfd,EPOLL_CTL_ADD,client_fd,&ev);
                if(status==-1)
                   {
                        fprintf(stderr, "Failed to add epoll client event\n");
                   }                                              //Summary of steps: 1.Create a new client socket and link it with this client(sensor);2.Add this socket into epoll monitor-dplist
            }
            //Situation2: Existing client input data
            else if(events[i].events&EPOLLIN)
            {
               
                current_element=((socket_with_sensor*)(events[i].data.ptr));            //current_element is used to translate the void pointer and access the socket and sensor by struct "socket_with_sensor"
                current_sock=(current_element->connection_sock);
                bytes = sizeof(data.id);
                result=tcp_receive(current_sock,(void *)&data.id,&bytes);            // read temperature to inter-buffer-data.id
                
                bytes = sizeof(data.value);
                result=tcp_receive(current_sock,(void *)&data.value,&bytes);         // read temperature to inter-buffer-data.value
                
                bytes = sizeof(data.ts);
                result=tcp_receive(current_sock, (void *)&data.ts,&bytes);           // read timestamp to inter-buffer-data.ts
                
                //1.TCP_NO_ERROR; 2.TCP_CONNECTION_CLOSED; 3.else error situation
                if (result==TCP_NO_ERROR)
                {
                   //read data from inter-buffer-data to sbuffer-buffer
                   int buffer_size=sbuffer_size(*buffer);
                   if(buffer_size<=SBUFFER_SIZE_MIN)                                //only do thread-connmgr and stop two read threads
                   {
                       data_noread_mutex_status= pthread_mutex_trylock( data_noread_mutex);     //In case one of two read threads already lock the mutex(for example the size is at the boundary of min value)
                       fprintf(stderr, "trylock once \n");
                       sbuffer_insert(*buffer,&data);
                   }
                   else if(buffer_size>=SBUFFER_SIZE_MAX)
                   {
                       int sleep_times=1;
                       while(buffer_size>=SBUFFER_SIZE_MAX)
                       {
                           sleep_times++;
                           int sleep_time=sleep_times*100000;        //each time sleep(0.1*sleep_times)-s
                           if (data_noread_mutex_status==0 ||data_noread_mutex_status==EBUSY)         //judge before unlock data_noread_mutex in case its already been unlocked
                           {
                               pthread_mutex_unlock( data_noread_mutex);
                               data_noread_mutex_status= 1;
                           }
                           usleep(sleep_time);
                           printf("connmgr sleep\n");
                           buffer_size=sbuffer_size(*buffer);   //refresh sbuffer_size for next loop judge
                       }
                   }
                   else                  //sbuffer_size is within the setting range
                   {
                       if (data_noread_mutex_status==0 ||data_noread_mutex_status==EBUSY)         //judge before unlock data_noread_mutex in case its already been unlocked
                       {
                           pthread_mutex_unlock( data_noread_mutex);                              //therefore two read thread can access the buffer
                           data_noread_mutex_status= 1;
                       }
                       sbuffer_insert(*buffer,&data);
                       
                   }
                  
    //Maybe the sensor information will not be put in the sbuffer(sbuffer > MAX),but it is mandatory to refresh it in the dplist，because it is useful to shut down the socket by judge the last active timestamp of each socket
                 current_element->record.id=data.id;
                 current_element->record.value=data.value;
                 current_element->record.ts=data.ts;
                 current_element->data_input_times++;
                 fprintf(stdout,"sensor id = %" PRIu16 " - temperature = %g - timestamp = %ld\n", data.id, data.value, (long int)data.ts);
                 printf("sbuffer size is %d\n",sbuffer_size(*buffer));
                }
                else if(result==TCP_CONNECTION_CLOSED)
                {
                    pthread_mutex_lock(FIFO_mutex);
                    FILE *fp_FIFO = fopen(FIFO_NAME, "w");
                    FILE_OPEN_ERROR(fp_FIFO);
                    char *send_buf;
                    char *pipe_buffer;
                    asprintf( &send_buf, "Peer sensor id= %d has closed connection\n",current_element->record.id);
                    asprintf( &pipe_buffer, "go on signal\n" );
                    write(pipe_write_fd, send_buf, strlen(send_buf)+1 );
                    if ( fputs( send_buf, fp_FIFO ) == EOF )             //send_buf into fifo
                    {
                        fprintf( stderr, "Error writing data to pipe\n");
                        exit( EXIT_FAILURE );
                    }
                    FFLUSH_ERROR(fflush(fp_FIFO));
                    free( send_buf );
                    free( pipe_buffer );
                    fclose(fp_FIFO);
                    pthread_mutex_unlock(FIFO_mutex);
                    epoll_ctl(epfd,EPOLL_CTL_DEL,current_sock->sd,NULL);
                }
                else
                {
                 printf("Error occured on connection to peer\n");
                 epoll_ctl(epfd, EPOLL_CTL_DEL,current_sock->sd,NULL);
                 int index = dpl_get_index_of_element(socket_dplist,current_element);
                 dpl_remove_at_index(socket_dplist,index,true);
                 tcp_close(&current_sock);
                 conn_counter--;
                }
                                       
            }
        } //end of for loop
        
        
        //Loop through the dplist to judge whether to shut some timeout socket of them
            for(int i=1;i<dpl_size(socket_dplist);i++)          //The first node is server socket,thats why it doesn't have to be judge
            {
                void *element = dpl_get_element_at_index(socket_dplist,i);
                current_element=((socket_with_sensor*)(element));
                current_sock=(current_element->connection_sock);
                if(current_element->data_input_times==1)            ///Meaning this client is new coming and there should be a log event
                {
                    current_element->data_input_times++;
                    pthread_mutex_lock(FIFO_mutex);
                    FILE *fp_FIFO = fopen(FIFO_NAME, "w");
                    FILE_OPEN_ERROR(fp_FIFO);
                    char *send_buf;
                    char *pipe_buffer;
                    asprintf( &send_buf, "A sensor node %d has opened a new connection\n",current_element->record.id);
                    asprintf( &pipe_buffer, "go on signal\n" );             //signal sent to child process by pipe to avoid block of function "read" of pipe
                    write(pipe_write_fd, send_buf, strlen(send_buf)+1 );
                    if ( fputs( send_buf, fp_FIFO ) == EOF )             //message from send_buf into fifo
                    {
                       fprintf( stderr, "Error writing data to pipe\n");
                       exit( EXIT_FAILURE );
                    }
                    FFLUSH_ERROR(fflush(fp_FIFO));
                    free( send_buf );
                    free( pipe_buffer );
                    fclose(fp_FIFO);
                    pthread_mutex_unlock(FIFO_mutex);
                }
                if((time(NULL)-(current_element->record.ts)) > TIMEOUT )
                {
                 printf("Socket %d is time out and deleted\n",(current_sock->sd));
                 //epoll_ctl(epfd, EPOLL_CTL_DEL, (current_sock->sd),NULL);
                 dpl_remove_at_index(socket_dplist,i,true);
                 tcp_close(&current_sock);
                 conn_counter--;
                 i--;                      //Cause one node is removed just now therefore it is needed to minus one for the next loop
                }
            }
        int iii=dpl_size(socket_dplist);
        printf("dpsize(client+server) is %d\n",iii);

        //if there is no client left，go judging whether to shut down the server
         if(conn_counter==0)
           {
               int outcome=0;                                              //used to receive number of active socket
               outcome=epoll_wait(epfd,events,MAX_CONN,time_s);           //if there is no signal coming in anather TIMEOUT-s, return 0 and go shut down the server
               if(outcome==0)
               {
                 printf("Waiting time out at second epoll_wait\n");
                   for(int i=0;i<dpl_size(socket_dplist);i++)               //Loop through dplist to free all the resource(socket memory in heap)
                   {
                       socket_with_sensor *current_element=NULL;
                       tcpsock_t * current_sock=NULL;
                       current_element=dpl_get_element_at_index(socket_dplist,i);
                       current_sock=current_element->connection_sock;
                       tcp_close(&current_sock);
                   }
                   printf("Test server is shutting down\n");
                   if (data_noread_mutex_status==0 ||data_noread_mutex_status==EBUSY)        //make sure two read thread can work after server shutting down
                   {
                       pthread_mutex_unlock( data_noread_mutex);
                       data_noread_mutex_status= EBUSY +1;
                   }
                   return;
               }     //end of if(outcome==0)
               
            //the outcome is not zero which means there is a new connection request
             else if(events[0].data.fd == request_fd)
              {
                  if (tcp_wait_for_connection(server,&client)!=TCP_NO_ERROR) exit(EXIT_FAILURE);
                   printf("Incoming client connection\n");
                   conn_counter++;     //one more socket
                   inter=(socket_with_sensor *)create_element();
                   inter->connection_sock=client;
                   inter->record.id=0;
                   inter->record.value=0;
                   inter->record.ts=0;
                   dpl_insert_at_index(socket_dplist,inter,dpl_size(socket_dplist),false);
                   tcp_get_sd(client,&client_fd);
                   ev.data.fd= client_fd;
                   ev.data.ptr=inter;
                   ev.events=EPOLLIN;
                   status= epoll_ctl(epfd,EPOLL_CTL_ADD,client_fd,&ev);
                  if(status==-1)
                  {
                       fprintf(stderr, "Failed to add epoll client event\n");
                  }
               }
             }   // end of if(conn_counter==0)
        }           //end of while-loop
       

    }  //function -final
                             

 void connmgr_free()
 {
     
     dpl_free(&socket_dplist,true);
 }
                             
 void* con_element_copy(void * element)
 {
     socket_with_sensor* new = NULL;
     new = malloc(sizeof(socket_with_sensor));
     *new = *(socket_with_sensor *)element;
     return ((void*)new);
 }

 void con_element_free(void **element)
 {
     socket_with_sensor* tag;
     tag=(socket_with_sensor*)(*element);
     free(tag);
     tag=NULL;
 }

 int con_element_compare(void * x, void * y)
 {
      socket_with_sensor *current_element_x=NULL;
      socket_with_sensor *current_element_y=NULL;
      current_element_x=(socket_with_sensor *)x;
      current_element_y=(socket_with_sensor *)y;
     if((current_element_x->connection_sock)->sd ==(current_element_y->connection_sock)->sd )
     {
         return 0;
     }
     else if((current_element_x->connection_sock)->sd < (current_element_y->connection_sock)->sd)
     {
            return -1;
     }
     else if((current_element_x->connection_sock)->sd > (current_element_y->connection_sock)->sd)
     {
              return 1;
     }
     return 0;
 }
void* create_element()
{
    socket_with_sensor *inter = (socket_with_sensor*)malloc(sizeof(socket_with_sensor));
     memset(inter, 0, sizeof(socket_with_sensor));
    inter->connection_sock=NULL;
    inter->record.id=0;
    inter->record.value=0;
    inter->record.ts=time(NULL);
    inter->data_input_times=0;
    return ((void*)inter);
}








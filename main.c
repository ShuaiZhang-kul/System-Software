#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/stat.h>
#include "sbuffer.h"
#include "connmgr.h"
#include "datamgr.h"
#include "sensor_db.h"
#include "errmacros.h"



#define PORT 5678
//shuai zhang-29/12/2019

// mutex declare
pthread_mutex_t data_noread_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t data_read_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t FIFO_mutex=PTHREAD_MUTEX_INITIALIZER;

//thread connmgr running flag declare
int thread_conn_flag=0;

//arg struct sent to each thread
typedef struct {
        int pipe_write_fd;
        int server_port ;
        sbuffer_t  *buffer;
}arg_connmgr;
typedef struct {
        int pipe_write_fd;
        FILE * fd_map_sensor;
        sbuffer_t  *buffer;
}arg_datamgr;
typedef struct {
        int pipe_write_fd;
        sbuffer_t  *buffer;
        char flag;
}arg_sql;


//each thread function
void *connmgr(void *arg_con)
{
    int port_num=((arg_connmgr*)arg_con)->server_port ;
    sbuffer_t *buffer=((arg_connmgr*)arg_con)->buffer;
    int pipe_write_fd= ((arg_connmgr*)arg_con)->pipe_write_fd;
    connmgr_listen(port_num,&buffer,&data_noread_mutex,&FIFO_mutex,pipe_write_fd);
    connmgr_free();
    printf("thread_connmgr is closing\n");
    thread_conn_flag=0;
    pthread_exit( NULL );
}
void *datamgr(void *arg_da)
{
    sbuffer_t *buffer=((arg_datamgr*)arg_da)->buffer;
    FILE *map_fd =((arg_datamgr*)arg_da)->fd_map_sensor;
    int pipe_write_fd= ((arg_datamgr*)arg_da)->pipe_write_fd;
    datamgr_parse_sensor_files(map_fd,&buffer,&data_noread_mutex,&data_read_mutex,&FIFO_mutex,pipe_write_fd,&thread_conn_flag);
    datamgr_free();
    printf("thread_datamgr is closing\n");
    pthread_exit( NULL );
}
void *sql(void *arg_sq)
{
    char clear_up_flag=((arg_sql*)arg_sq)->flag;
    sbuffer_t *buffer=((arg_sql*)arg_sq)->buffer;
    int pipe_write_fd= ((arg_sql*)arg_sq)->pipe_write_fd;
    DBCONN * conn=init_connection(clear_up_flag,&FIFO_mutex,pipe_write_fd);
    storagemgr_parse_sensor_data(conn, &buffer,&data_noread_mutex,&data_read_mutex,&FIFO_mutex,pipe_write_fd,&thread_conn_flag);       //read data from sbuffer continiously
    disconnect(conn,&FIFO_mutex,pipe_write_fd);
    printf("thread_sql is closing\n");
    pthread_exit( NULL );
}
int main(int argc, char *argv[])
{
    
    //Initialize variables
    int port;
    sbuffer_t *buffer=NULL;
    int sbuffer_result;
    pthread_t thread_connmgr,thread_sql,thread_datamgr;
    FILE * fp_sensor_map= fopen("room_sensor.map","r");                         //Get mapping between Sensor_ID and Room_ID
    FILE_OPEN_ERROR(fp_sensor_map);
    sbuffer_result=sbuffer_init(&buffer);                                       //Create and initiate buffer for later usage
    SBUFFER_ERROR(sbuffer_result);
    
    //Get port from commond line( Function structure is cited from lecture template code)
    if (argc != 2)
    {
        printf("Please give the port number in the commond line\n");
        exit(-1);
    }
    else
    {
        port = atoi(argv[1]);
        printf("Port number input success\n");
    }
    
    
    //Create a normal-pipe and a FIFO-pipe
    int pfds[2];
    int result = pipe(pfds);                                //nomal pipe is used to inform child process if the parent process is still running
    CHECK_PIPE(result);
    result = mkfifo(FIFO_NAME, 0666);                       //FIFO pipe is used to transmit the String data from parent process to child process
    CHECK_MKFIFO(result);
    
    //initiate arg struct sent for each thread
    arg_connmgr arg_con;
    arg_datamgr arg_da;
    arg_sql arg_sq;
    arg_con.server_port=port;
    arg_con.buffer=buffer;
    arg_con.pipe_write_fd= pfds[1];
    arg_da.fd_map_sensor=fp_sensor_map;
    arg_da.buffer=buffer;
    arg_da.pipe_write_fd= pfds[1];
    arg_sq.flag=1;
    arg_sq.buffer=buffer;
    arg_sq.pipe_write_fd= pfds[1];
    
    //Create child process here
    pid_t pid;
    pid=fork();                                         //Variables created before this line will be duplicated into double(will not duplicate but stay with one for only-read varibles)
    
    //Fork create failed
    if(pid<0)
    {
        perror("fork failed");
        exit(1);
    }
    //Child process
    if(pid==0)
    {
        close(pfds[1]);                //Close the write side of pipe from child
        int sequence_number=0;         //Indicating the sequence number by loop number
        while(1)                       //Each time means a new log event
        {
        FILE *fp,*fp_gateway;
        char *str_result;
        char FIFO_recv_buf[MAX];
        char pipe_recv_buf[MAX];
        fp = fopen(FIFO_NAME, "r");               //Block-function for read untill there is a new LOG event happens and open the FIFO from parent process side
        FILE_OPEN_ERROR(fp);
        printf("syncing with writer ok\n");
        fp_gateway=fopen(LOG_NAME,"at");          //Test-file, wirte at the end
//printf("111111111111111111111111123\n");
        //FILE_OPEN_ERROR(fp_gateway);
        char * head_buffer;                       //Used to add head of log event
        int pipe_result = read(pfds[0], pipe_recv_buf, MAX);        //It doesnt matter what parent send,just used to detect if there is a signal from main process to shut child proxess down(0 byte readed if shut down)
  //printf("2222222222222222222222222\n");     
 if(pipe_result==0)                                          //CLose child process
        {
            result = fclose(fp);
            FILE_CLOSE_ERROR(result);
            result = fclose(fp_gateway);
            FILE_CLOSE_ERROR(result);
            result = fclose(fp_sensor_map);
            FILE_CLOSE_ERROR(result);
            sbuffer_result=sbuffer_free(&buffer);
            SBUFFER_FREE_ERROR(sbuffer_result);
            close( pfds[0] );
            str_result=NULL;
            free(str_result);
            break;                                              //Break from while-loop and go to exit(0) at line-198.this is the only way to exit child process
        }
       do
        {
            str_result = fgets(FIFO_recv_buf, MAX, fp);        //Get data from FIFO pipe to FIFO_recv_buf
            if ( str_result != NULL )
           {
             asprintf( &head_buffer, "<%05d> ", sequence_number );
             fputs(head_buffer,fp_gateway);
             free(head_buffer);
             struct tm* local;
             time_t t = time(NULL);
             local = localtime(&t);
             char *a=asctime(local);
             a[strlen(a)-1]=0;
             asprintf(&head_buffer,"<%s> <EVENT>:",a);
             fputs(head_buffer,fp_gateway);
             FFLUSH_ERROR(fflush(fp_gateway));
             free(head_buffer);
             fputs(FIFO_recv_buf,fp_gateway);                  //Write data from FIFO_recv_buf to log file
             FFLUSH_ERROR(fflush(fp_gateway));
            }
         }while ( str_result != NULL );
        result = fclose( fp );
        FILE_CLOSE_ERROR(result);
        result = fclose( fp_gateway );
        FILE_CLOSE_ERROR(result);
        sequence_number++;
        free(str_result);
        }
       exit(0);                                                               //Exit of child process
    }                                                                         //End of " if(pid==0)"
    
    //Main process
    else
    {
        int presult;
        close(pfds[0]);                                                            //Close the read side of pipe from child
        //pthread_create
        presult = pthread_create( &thread_connmgr, NULL, &connmgr, &arg_con );
        PTHREAD_CONNMGR_CREATE(presult);
        thread_conn_flag=1;                 //Indicating thread connmgr is running now
   
        presult = pthread_create( &thread_sql, NULL, &sql, &arg_sq );
        PTHREAD_SQL_CREATE(presult);
        
        presult = pthread_create( &thread_datamgr, NULL, &datamgr, &arg_da );
        PTHREAD_DATAMGR_CREATE(presult);
  
        //pthread_join
        presult= pthread_join(thread_connmgr, NULL);
        PTHREAD_CONNMGR_CLOSE(presult);

        presult= pthread_join(thread_sql, NULL);
        PTHREAD_SQL_CLOSE(presult);
    
        presult= pthread_join(thread_datamgr, NULL);
        PTHREAD_DATAMGR_CLOSE(presult);
        
        presult = pthread_mutex_destroy( &data_noread_mutex );
        PTHREAD_MUTEX_DESTROY(presult);
        presult = pthread_mutex_destroy( &data_read_mutex );
        PTHREAD_MUTEX_DESTROY(presult);
        
        sbuffer_result=sbuffer_free(&buffer);
        SBUFFER_FREE_ERROR(sbuffer_result);
            
        FILE *fp_FIFO = fopen(FIFO_NAME, "w");              //Make sure child process will not be blocked in Line-151 due to the blocking property of fopen
        
        //printf("3333333333333333333333\n");
        close(pfds[1]);             // Indicate end of writing so that child can terminate(by get a 0 return value)
	//printf("44444444444444444444444\n");
        waitpid(pid, NULL, 0);       //0 means blocking function
        printf("Child process terminated successfully\n");

        result=fclose(fp_sensor_map);
        FILE_CLOSE_ERROR(result);
        
        result=fclose(fp_FIFO);
        FILE_CLOSE_ERROR(result);
        
        presult = pthread_mutex_destroy( &FIFO_mutex );
        PTHREAD_MUTEX_DESTROY(presult);
    
        printf("Main process terminated successfully\n");
        pthread_exit(NULL);                //Exit of Thread-main
    }                                      //End of else
    
    printf("Whole program terminated successfully\n");
    exit(0);                            //Exit whole Program(not useful actually but for whole structure)
}                                       //End of main for parent process









#define _GNU_SOURCE
#include <pthread.h>
#include "datamgr.h"
#include <stdio.h>
#include "lib/dplist.h"
#include "errmacros.h"
#include <unistd.h>
#include <string.h>
#include "config.h"
#include "sbuffer.h"
#define data_fd 1                                   //This is the thread file description used to tell sbuffer which thread is read it

dplist_t * sensor_dplist = NULL;


void datamgr_parse_sensor_files(FILE * fp_sensor_map, sbuffer_t ** buffer,pthread_mutex_t* data_noread_mutex,pthread_mutex_t* data_read_mutex,pthread_mutex_t* FIFO_mutex,int pipe_write_fd,int* thread_conn_flag)

{
    //Creat a dplist to store information of each sensor
    ERROR_HANDLER(fp_sensor_map==NULL,"fp_sensor_map==NULL");
    sensor_dplist = dpl_create(&data_element_copy,&data_element_free,&data_element_compare);
    ERROR_HANDLER(sensor_dplist==NULL,"sensor_dplist==NULL");
    
    room_id_t room_ID;
    sensor_id_t sensor_ID;
 
    //Read all the sensor-id and corresponding room-id then create a node for this sensor-room.Add this node into the dplist
    
    while(fscanf(fp_sensor_map, "%hu %hu" , &room_ID,&sensor_ID)!=EOF)
    {
        int i=0;
        element_t *a =NULL;
        a = malloc( sizeof(element_t) );
        memset(a, 0, sizeof(element_t));
        ERROR_HANDLER(a==NULL,"memory allocation problems");
        a->room_ID=room_ID;
        a->sensor_ID=sensor_ID;
        a->average=0;
        a->ts=0;
        dpl_insert_at_index(sensor_dplist,a,i,false);
        i++;
    }
                          //finish reading ensor_map file, and start to read sbuffer(instead of file 'fp_sensor_data')
        while(1)                 //endless loop to read data from sbuffer
     {
         
         int result =0;
         int sleep_times=1;
         result = pthread_mutex_lock( data_noread_mutex );         //This step is used to detect if conn_mgr thread allow two read thread to read(competition between two read and the write thread)
         CHECK_MUTEX_LOCK(result);
       
         result = pthread_mutex_unlock(data_noread_mutex );        //Once data_mgr thread find it can read the sbuffer,unlock the mutex for another read thread(sql_mgr) to detect
         CHECK_MUTEX_UNLOCK(result);
         
         result = pthread_mutex_lock( data_read_mutex );           //"data_read_mutex" is used for the competation between two read thread,this can make sure two read thread wont work at same time(For erasing the last node)
         CHECK_MUTEX_LOCK(result);
         
         fprintf(stdout,"Data_mgr read new loop\n");
         printf("sbuffer size is %d\n",sbuffer_size(*buffer));
         //printf("datamgr get the access to read \n");
         if(sbuffer_size(*buffer)<SBUFFER_SIZE_MIN && (*thread_conn_flag)==1)       //If the sbuffer size is smaller than MIN,thread sleep untill buffer-size bigger than the threshold(no sleep anymore if conn_mgr is closed)
         {
             result = pthread_mutex_unlock(data_read_mutex );                   //Give the access to sql_read thread(sql will also sleep)
             CHECK_MUTEX_UNLOCK(result);
             
             while(sbuffer_size(*buffer)<SBUFFER_SIZE_MIN && (*thread_conn_flag)==1)
             {
                 usleep(600000);
                 printf("datamgr sleeptime is %d\n",sleep_times);
                 sleep_times++;                //used for thread connmgr has closed
             }
             sleep_times=0;
             result = pthread_mutex_lock( data_noread_mutex );          //Detect if connmgr allow datamgr continue again (back-up judgement even though the same function as sbuffer size bigger than MIN)
             CHECK_MUTEX_LOCK(result);                                  //Restart as in the beginning at line-49
             
             result = pthread_mutex_unlock(data_noread_mutex );
             CHECK_MUTEX_UNLOCK(result);
             
             result = pthread_mutex_lock( data_read_mutex );
             CHECK_MUTEX_LOCK(result);
         }
         if(sbuffer_size(*buffer)==0)                               //No data in the buffer, which means its time to shut down data_mgr thread
         {
             result = pthread_mutex_unlock(data_read_mutex );       //Make sure Sql thread can get the access of CPU if data_mgr thread exit first
             return;
         }
         int datamgr_fd= data_fd;
         int remove_status=0;
         sensor_data_t data;
         data.id=0;
         data.value=0;
         data.ts=0;          //Intermediate variables
         
            //start to judge the status of remove
         remove_status=sbuffer_remove(*buffer,&data,datamgr_fd);            //Read first and detect if it is time to delete this node(if sql_mgr thread already read this node)
         
         if (remove_status==(SBUFFER_NO_DATA))                              //This situation might not happen because it will return at line-85 if buffer size is zero
         {
             result = pthread_mutex_unlock( data_read_mutex );
             printf("no data in the buffer");
             return;         //finish this thread
         }
         if (remove_status==(SBUFFER_REPEAT_READ))                          //If read this node continuously,sleep to make sure another read thread can get the access to read the node in order to delete it and go next
         {
            result = pthread_mutex_unlock( data_read_mutex );               //Give sql thread the access
            printf("datamgr sleep\n");
            usleep(500000);             //this thread sleep 0.1s which means another read thread can wake up to read
        }
        if (remove_status==(SBUFFER_SUCCESS))           //Two possibility:1.The first thread to read this node,then get the information only and keep the node.
        {                                                       //        2.The second thread to read this node,then get the information and delete the node.
             printf("remove_status=success\n");
            result = pthread_mutex_unlock( data_read_mutex );
            element_t *b =NULL;
            b=get_element_of_sensor_ID(data.id,FIFO_mutex,pipe_write_fd);
            b->sensor_ID=data.id;
            b->ts=data.ts;
            b->record_times=((b->record_times)+1);
            b->average = 0;
            //b->room_ID=b->room_ID;
            b->average=refresh_array(b,data.value);
            
            //Dangerous situation 1: Temperature is too high
            if((b->average)>SET_MAX_TEMP)
            {
                pthread_mutex_lock(FIFO_mutex);
                FILE *fp_FIFO = fopen(FIFO_NAME, "w");
                FILE_OPEN_ERROR(fp_FIFO);
                char *send_buf;
                char *pipe_buffer;
                asprintf( &send_buf, "The sensor node with ID-%hu reports temperature in room %hu is too high %lf \n",b->sensor_ID,b->room_ID,b->average);
                asprintf( &pipe_buffer, "go on signal\n" );
                write(pipe_write_fd, send_buf, strlen(send_buf)+1 );
                if ( fputs( send_buf, fp_FIFO ) == EOF )
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
            
            //Dangerous situation 1: Temperature is too high
            if((b->average) < SET_MIN_TEMP)
            {
                if((b->record_times)>=RUN_AVG_LENGTH)
                {
                    pthread_mutex_lock(FIFO_mutex);
                    FILE *fp_FIFO = fopen(FIFO_NAME, "w");
                    FILE_OPEN_ERROR(fp_FIFO);
                    char *send_buf;
                    char *pipe_buffer;
                    asprintf( &send_buf, "The sensor node with ID-%hu reports temperature in room %hu is too low %lf \n",b->sensor_ID,b->room_ID,b->average);
                    asprintf( &pipe_buffer, "go on signal\n" );
                    write(pipe_write_fd, send_buf, strlen(send_buf)+1 );
                    if ( fputs( send_buf, fp_FIFO ) == EOF )
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
            }
        }                           //End of "if (remove_status==(SBUFFER_SUCCESS))"
      }                             //End of "while loop"
    }                               //End of function"datamgr_parse_sensor_files"
        


void datamgr_free(void)
{
     dpl_free( &sensor_dplist,true);
    sensor_dplist=NULL;
}

element_t* get_element_of_sensor_ID(sensor_id_t sensor_ID,pthread_mutex_t* FIFO_mutex,int pipe_write_fd)
{
    element_t *b =NULL;
    b = malloc( sizeof(element_t) );
    b->sensor_ID=sensor_ID;
    int index = dpl_get_index_of_element(sensor_dplist,b);
    if(index==-1)
    {
        pthread_mutex_lock(FIFO_mutex);
	FILE *fp_FIFO = fopen(FIFO_NAME, "w");   
        FILE_OPEN_ERROR(fp_FIFO);
        char *send_buf;
        char *pipe_buffer;
        asprintf( &send_buf, "invalid sensor ID %hu \n",sensor_ID);
        asprintf( &pipe_buffer, "go on signal\n" );
        write(pipe_write_fd, send_buf, strlen(send_buf)+1 );
        if ( fputs( send_buf, fp_FIFO ) == EOF )
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
    free(b);
    b = dpl_get_element_at_index(sensor_dplist,index);
    return b;
    
}

sensor_value_t refresh_array(element_t* current,sensor_value_t new)       //refresh the record array and return a new average value
{
    sensor_value_t inter_1 = new;
    sensor_value_t inter_2 =0;
    int i=(RUN_AVG_LENGTH-1);
    sensor_value_t new_ave=0;;
    while(i>=0)
    {
        inter_2=(current->record[i]);
        (current->record[i])=inter_1;
        inter_1=inter_2;
        i--;
    }
    i=0;
    while(i<=(RUN_AVG_LENGTH-1))
    {
        new_ave=(new_ave + (current->record[i]));
        i++;
    }
    new_ave=(new_ave/RUN_AVG_LENGTH);
    if((current->record_times)>=RUN_AVG_LENGTH)
    {
        return new_ave;
    }
    else
    {
        return 0;
    }
}

void* data_element_copy(void * element)
{
    element_t* new = NULL;
    new = malloc(sizeof(element_t));
    *new = *(element_t *)element;
    return ((void*)new);
}

void data_element_free(void **element)
{
    element_t* tag;
    tag=(element_t*)(*element);
    free(tag);
    tag=NULL;
}

int data_element_compare(void * x, void * y)
{
    if(((element_t*)(x))->sensor_ID==((element_t*)(y))->sensor_ID)
    {
        return 0;
    }
    else if(((element_t*)(x))->sensor_ID < ((element_t*)(y))->sensor_ID)
    {
           return -1;
    }
    else if(((element_t*)(x))->sensor_ID > ((element_t*)(y))->sensor_ID)
    {
             return 1;
    }
    return 0;
}







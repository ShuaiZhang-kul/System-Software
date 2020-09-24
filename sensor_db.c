//
//  sensor_db.c
//  Lab7
//
//  Created by shuai zhang on 2019/12/11.file 最终测试版
//  Copyright © 2019 shuai zhang. All rights reserved.
//
#define _GNU_SOURCE
#include "sensor_db.h"
#include <stdio.h>
#include <sqlite3.h>
#include <string.h>
#include <pthread.h>
#include "config.h"
#include <unistd.h>
#include <string.h>
#include "errmacros.h"

#define sql_fd 2                //This is the thread file description used to tell sbuffer which thread is read it

DBCONN * init_connection(char clear_up_flag,pthread_mutex_t* FIFO_mutex,int pipe_write_fd)
{
    sqlite3 *db;
    int rc;
    rc = sqlite3_open(TO_STRING(DB_NAME), &db);              //return 'SQLITE_OK' if succeed opening
    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }

    //Then start to report log to child process
    pthread_mutex_lock(FIFO_mutex);
    FILE *fp_FIFO = fopen(FIFO_NAME, "w");
    FILE_OPEN_ERROR(fp_FIFO);
    char *send_buf;
    char *pipe_buffer;
    asprintf( &send_buf, "Connection to SQL server established.\n");
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
    //Log report end
    
    char* errmsg=0;
    char* sql=malloc(800*sizeof(char));
    if(clear_up_flag==1)                            // which means If the table existed, clear up the existing data
    {
        sprintf(sql,"DROP TABLE IF EXISTS "TO_STRING(TABLE_NAME)";"\
                    "CREATE TABLE "TO_STRING(TABLE_NAME)" ( \
                    id INTEGER PRIMARY KEY AUTOINCREMENT,  \
                    sensor_id INT,  \
                    sensor_value DECIMAL(4,2),  \
                    timestamp TIMESTAMP);");                                   //write sql sentence
        rc=sqlite3_exec(db,sql,NULL,0,&errmsg);                  //excute sql sentence without callback function
        if( rc != SQLITE_OK )
        {
            fprintf(stderr, "SQL error: %s\n", errmsg);
            sqlite3_free(errmsg);
            free(sql);
            sqlite3_close(db);
            return NULL;
        }
        else            //Create successfully
        {
            pthread_mutex_lock(FIFO_mutex);
            FILE *fp_FIFO = fopen(FIFO_NAME, "w");
            FILE_OPEN_ERROR(fp_FIFO);
            char *send_buf;
            char *pipe_buffer;
            asprintf( &send_buf, "NEW table %s created successfully\n",TO_STRING(TABLE_NAME));
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
            free(sql);
        }
    }                                                //End of " if(clear_up_flag==1)  "
    else                                         //   which means If the table existed, do no create again
    {
        sprintf(sql,"CREATE TABLE IF NOT EXISTS "TO_STRING(TABLE_NAME)"(  \
                id INT PRIMARY KEY,  \
                sensor_id INT,  \
                sensor_value DECIMAL(4,2),  \
                timestamp TIMESTAMP);");                                   //write sql sentence
        rc=sqlite3_exec(db,sql,0,0,&errmsg);                  //excute sql sentence without callback function
        if( rc != SQLITE_OK )
        {
            fprintf(stderr, "SQL error: %s\n", errmsg);
            sqlite3_free(errmsg);
            free(sql);
            sqlite3_close(db);
            return NULL;
        }
        else
        {
            pthread_mutex_lock(FIFO_mutex);
            FILE *fp_FIFO = fopen(FIFO_NAME, "w");
            FILE_OPEN_ERROR(fp_FIFO);
            char *send_buf;
            char *pipe_buffer;
            asprintf( &send_buf, "NEW table %s created successfully\n",TO_STRING(TABLE_NAME));
            asprintf( &pipe_buffer, "go on signal\n" );    //先写到send_buf里边
            write(pipe_write_fd, send_buf, strlen(send_buf)+1 );
            if ( fputs( send_buf, fp_FIFO ) == EOF )             //从send_buf写到fifo文件里
            {
                fprintf( stderr, "Error writing data to pipe\n");
                exit( EXIT_FAILURE );
            }
            FFLUSH_ERROR(fflush(fp_FIFO));                   //执行一次fflush,fgets就会唤醒一次
            free( send_buf );
            free( pipe_buffer );
            fclose(fp_FIFO);
            pthread_mutex_unlock(FIFO_mutex);
            free(sql);
            }
        }                                    // //End of "else"
        return db;                           //return access address of db
}

void storagemgr_parse_sensor_data(DBCONN * conn, sbuffer_t ** buffer,pthread_mutex_t* data_noread_mutex,pthread_mutex_t* data_read_mutex,pthread_mutex_t*FIFO_mutex,int pipe_write_fd,int* thread_conn_flag)
{
 printf("thread_sql listen is opening\n");
    while(1)                                 //endless loop used to read data from sbuffer until there is no node left to read,return then.
     {
         int result = pthread_mutex_lock( data_noread_mutex );          //The same mechanism as data_mgr thread
         CHECK_MUTEX_LOCK(result);
        
         result = pthread_mutex_unlock( data_noread_mutex );        //Once data_mgr thread find it can read the sbuffer,unlock the mutex for another read thread(sql_mgr) to detect
         CHECK_MUTEX_UNLOCK(result);
         
         result = pthread_mutex_lock( data_read_mutex );     //"data_read_mutex" is used for the competation between two read thread,this can make sure two read thread wont work at same time(For erasing the last node)
         CHECK_MUTEX_LOCK(result);
         
         fprintf(stdout,"sql read new loop\n");
         printf("sbuffer size is %d\n",sbuffer_size(*buffer));
         if(sbuffer_size(*buffer)<SBUFFER_SIZE_MIN && (*thread_conn_flag)==1) //If the sbuffer size is smaller than MIN,thread sleep untill buffer-size bigger than the threshold(no sleep anymore if conn_mgr is closed)
         {
             int sleep_times=1;
             result = pthread_mutex_unlock(data_read_mutex );
             while(sbuffer_size(*buffer)<SBUFFER_SIZE_MIN && (*thread_conn_flag)==1)
             {
                 usleep(600000);
                 printf("sql sleeptime is %d\n",sleep_times);
                 sleep_times++;                //used for thread connmgr has closed
             }
             result = pthread_mutex_lock( data_noread_mutex );  //Detect if connmgr allow datamgr continue again (back-up judgement even though the same function as sbuffer size bigger than MIN)
             CHECK_MUTEX_LOCK(result);
            
             result = pthread_mutex_unlock(data_noread_mutex );
             CHECK_MUTEX_UNLOCK(result);
             
             result = pthread_mutex_lock( data_read_mutex );   //Get the access to buffer again
             CHECK_MUTEX_LOCK(result);
         }
         if(sbuffer_size(*buffer)==0)           //When connmgr shut down and sbuffer is empty,return
         {
             result = pthread_mutex_unlock(data_read_mutex );
             return;
         }
         int sensor_db_fd= sql_fd;
         int remove_status=0;
         sensor_data_t data;
         data.id=0;
         data.value=0;
         data.ts=0;             //Intermediate variables
         
           //start to judge the status of remove
         remove_status=sbuffer_remove(*buffer,&data,sensor_db_fd);
         if (remove_status==(SBUFFER_NO_DATA))                  //Actually,not possible to happen but for insurance
         {
           result = pthread_mutex_unlock( data_read_mutex );
           return;         //finish this thread
         }
         if (remove_status==(SBUFFER_REPEAT_READ))    //If read this node continuously,
							//sleep to make sure another read thread can get
							// the access to read the node in order to delete it and go next
         {
             result = pthread_mutex_unlock(data_read_mutex );
             printf("sql sleep\n");
             usleep(600000);                             //this thread sleep 0.6s which means another read thread can wake up to read
         }
         if (remove_status==(SBUFFER_SUCCESS))          //Two possibility:1.The first thread to read this node,then get the information only and keep the node.
         {                                                       //        2.The second thread to read this node,then get the information and delete the node.
            // insert_sensor(conn, data.id, data.value, data.ts);
             printf("sql remove success\n");
             result = pthread_mutex_unlock( data_read_mutex );
             insert_sensor(conn,data.id,data.value,data.ts);
         }
        }           //End of "while loop"
}                   //End of function "storagemgr_parse_sensor_data"

int insert_sensor(DBCONN * conn, sensor_id_t id, sensor_value_t value, sensor_ts_t ts)
{
    
    char* errmsg=NULL;
    int rc;
    char* sql=malloc(500*sizeof(char));
    memset(sql, 0, 500*sizeof(char));
    sprintf(sql,"INSERT INTO "TO_STRING(TABLE_NAME)" (sensor_id,sensor_value,timestamp) "
    "VALUES (@id, @value, @ts);");
    rc=sqlite3_exec(conn,sql,NULL,NULL,&errmsg);                  //excute sql sentence without callback function
    if( rc != SQLITE_OK ){
    fprintf(stderr, "SQL insert error: %s\n", errmsg);
    sqlite3_free(errmsg);
    free(sql);
    return -1;}
    sqlite3_free(errmsg);
    free(sql);
    return 0;                           //return access address of db
}
void disconnect(DBCONN *conn,pthread_mutex_t*FIFO_mutex,int pipe_write_fd)
{
    sqlite3_close(conn);
    pthread_mutex_lock(FIFO_mutex);
    FILE *fp_FIFO = fopen(FIFO_NAME, "w");
    FILE_OPEN_ERROR(fp_FIFO);
    char *send_buf;
    char *pipe_buffer;
    asprintf( &send_buf, "Connection to SQL server lost\n");
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



//final-version



#ifndef DATAMGR_H_
#define DATAMGR_H_

#include <stdlib.h>
#include <stdio.h>
#include "config.h"
#include "sbuffer.h"

#ifndef RUN_AVG_LENGTH
  #define RUN_AVG_LENGTH 5
#endif

#ifndef SET_MAX_TEMP
  #define SET_MAX_TEMP 20
#endif

#ifndef SET_MIN_TEMP
  #define SET_MIN_TEMP 17
#endif

/*
 * Use ERROR_HANDLER() for handling memory allocation problems, invalid sensor IDs, non-existing files, etc.
 */
#define ERROR_HANDLER(condition,...)     do { \
                      if (condition) { \
                        printf("\nError: in %s - function %s at line %d: %s\n", __FILE__, __func__, __LINE__, __VA_ARGS__); \
                        exit(EXIT_FAILURE); \
                      }    \
                    } while(0)

/*
 *  This method holds the core functionality of your datamgr. It takes in 2 file pointers to the sensor files and parses them.
 *  When the method finishes all data should be in the internal pointer list and all log messages should be printed to stderr.
 */

/*
 * This method should be called to clean up the datamgr, and to free all used memory.
 * After this, any call to datamgr_get_room_id, datamgr_get_avg, datamgr_get_last_modified or datamgr_get_total_sensors will not return a valid result
 */
void datamgr_free(void);//释放链表里的所有内存，文件头结构不需要释放，这个是main函数里释放的
    




/*
 *  Return the total amount of unique sensor ID's recorded by the datamgr
 */
int datamgr_get_total_sensors(void);             //给出有多少个sensor id

int data_element_compare(void * x, void * y);

void* data_element_copy(void * element);

void data_element_free(void **element);


sensor_value_t refresh_record_array(sensor_value_t new);

void datamgr_parse_sensor_files(FILE * fp_sensor_map, sbuffer_t ** buffer,pthread_mutex_t* data_noread_mutex,pthread_mutex_t* data_read_mutex,pthread_mutex_t* FIFO_mutex,int pipe_write_fd,int* thread_conn_flag); 

typedef uint16_t room_id_t;
typedef struct                      //每个node的结构
{
  sensor_id_t sensor_ID;
  room_id_t  room_ID;
  sensor_value_t record[RUN_AVG_LENGTH];
  sensor_value_t average;
  sensor_ts_t ts;
  int record_times;
} element_t;

sensor_value_t refresh_array(element_t* current,sensor_value_t new);
element_t* get_element_of_sensor_ID(sensor_id_t sensor_ID,pthread_mutex_t* FIFO_mutex,int pipe_write_fd);


#endif  //DATAMGR_H_



#ifndef _SBUFFER_H_
#define _SBUFFER_H_

#include "config.h"

#define SBUFFER_FAILURE -1
#define SBUFFER_SUCCESS 0
#define SBUFFER_NO_DATA 1
#define SBUFFER_REPEAT_READ 2

#define SBUFFER_SIZE_MAX 25
#define SBUFFER_SIZE_MIN 8
typedef struct sbuffer sbuffer_t;


/*
 * Allocates and initializes a new shared buffer
 * Returns SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occured
 */
int sbuffer_init(sbuffer_t ** buffer); //建立头结构,输入的是一个main函数的头结构指针的"地址"


/*
 * All allocated resources are freed and cleaned up
 * Returns SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occured
 */
int sbuffer_free(sbuffer_t ** buffer);


/*
 * Removes the first sensor data in 'buffer' (at the 'head') and returns this sensor data as '*data'  
 * 'data' must point to allocated memory because this functions doesn't allocated memory
 * If 'buffer' is empty, the function doesn't block until new sensor data becomes available but returns SBUFFER_NO_DATA
 * Returns SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occured
 */
int sbuffer_remove(sbuffer_t * buffer,sensor_data_t * data,int function_fd);//直接传入指针变量的值也就是main函数*buffer变量的buffer
//移除第一个node,并把该node里的data移到*data里

int sbuffer_size( sbuffer_t * buffer ) ;
// Returns the number of elements in the list.
/* Inserts the sensor data in 'data' at the end of 'buffer' (at the 'tail')
 * Returns SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occured
*/
int sbuffer_insert(sbuffer_t * buffer, sensor_data_t * data);
//在末尾添加node


#endif  //_SBUFFER_H_


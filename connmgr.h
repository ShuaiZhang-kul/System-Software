#ifndef CONNMGR_H
#define CONNMGR_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/time.h>
#include <assert.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include "config.h"
#include "lib/tcpsock.h"
#include "lib/dplist.h"
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <errno.h>
#include "sbuffer.h"



struct tcpsoc_sensor {
tcpsock_t *connection_sock;             //用来存该sensor对应的socket地址
sensor_data_t record;                   //用来存该sensor对应的数据
int data_input_times;
} ;

typedef struct tcpsoc_sensor socket_with_sensor;

/*
 * This method starts listening on the given port and when when a sensor node connects it 
 * stores the sensor data in the shared buffer.
 */

void connmgr_listen(int port_number, sbuffer_t ** buffer,pthread_mutex_t *data_noread_mutex,pthread_mutex_t *FIFO_mutex,int pipe_write_fd);
/*
 * This method should be called to clean up the connmgr, and to free all used memory. 
 * After this no new connections will be accepted
 */


void connmgr_free(void);

void* con_element_copy(void * element);

void con_element_free(void **element);

int con_element_compare(void * x, void * y);

void* create_element(void);

#endif /* CONNMGR_H */


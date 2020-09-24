#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "sbuffer.h"


typedef struct sbuffer_node {
  struct sbuffer_node * next;
  sensor_data_t data;
  int read_times;
  int func_fd;           //indicate which one is reading this node last time
} sbuffer_node_t;

struct sbuffer {
  sbuffer_node_t * head;
  sbuffer_node_t * tail;
};	


int sbuffer_init(sbuffer_t ** buffer)
{
  *buffer = malloc(sizeof(sbuffer_t));
  if (*buffer == NULL) return SBUFFER_FAILURE;
  (*buffer)->head = NULL;
  (*buffer)->tail = NULL;
  return SBUFFER_SUCCESS; 
}


int sbuffer_free(sbuffer_t ** buffer)
{
  sbuffer_node_t * dummy;
  if ((buffer==NULL) || (*buffer==NULL)) 
  {
    return SBUFFER_FAILURE;
  } 
  while ( (*buffer)->head )
  {
    dummy = (*buffer)->head;
    (*buffer)->head = (*buffer)->head->next;
    free(dummy);
  }
  free(*buffer);
  *buffer = NULL;
  return SBUFFER_SUCCESS;		
}


int sbuffer_remove(sbuffer_t * buffer,sensor_data_t * data,int function_fd)
{
  sbuffer_node_t * dummy;
  int read_times_of_this_node = (buffer->head->read_times);
  int last_read_func_fd=(buffer->head->func_fd);
  if (buffer == NULL) return SBUFFER_FAILURE;
  if (buffer->head == NULL) return SBUFFER_NO_DATA;
  *data = buffer->head->data;
  (buffer->head->read_times)=((buffer->head->read_times)+1);
  (buffer->head->func_fd)=function_fd;
  read_times_of_this_node=(buffer->head->read_times);
 printf("last_read_func_fd %d\n",last_read_func_fd);
 printf("function_fd %d\n",function_fd);
  if (function_fd==last_read_func_fd)                   //same function reading this node comparing to last time
    {
        return SBUFFER_REPEAT_READ;
    }
  if (read_times_of_this_node>=2 && function_fd!= last_read_func_fd )          //remove this node
    {
	printf("read_times_of_this_node= %d\n",read_times_of_this_node);
        dummy = buffer->head;
        if (buffer->head == buffer->tail) // buffer has only one node
        {
            buffer->head = buffer->tail = NULL;
        }
        else     // buffer has many nodes empty
        {
            buffer->head = buffer->head->next;
        }
	  printf("remove one node\n");
        free(dummy);
        return SBUFFER_SUCCESS;                          //for the second read of this node by two different function,delete the head and change next node as head then
    }
printf("read_times_of_this_node= %d\n",read_times_of_this_node);
  return SBUFFER_SUCCESS;                               //for the first time read,also return SBUFFER_SUCCESS
}


int sbuffer_insert(sbuffer_t * buffer, sensor_data_t * data)
{
  sbuffer_node_t * dummy;
  if (buffer == NULL) return SBUFFER_FAILURE;
  dummy = malloc(sizeof(sbuffer_node_t));
  if (dummy == NULL) return SBUFFER_FAILURE;
  dummy->data = *data;
  dummy->read_times = 0;
  dummy->func_fd=0;
  dummy->next = NULL;
  if (buffer->tail == NULL) // buffer empty (buffer->head should also be NULL
  {
    buffer->head = buffer->tail = dummy;
  } 
  else // buffer not empty
  {
    buffer->tail->next = dummy;
    buffer->tail = buffer->tail->next; 
  }
  return SBUFFER_SUCCESS;
}
int sbuffer_size( sbuffer_t * buffer )
{
  // add your code here
    int count;
    sbuffer_node_t * dummy=buffer->head;
    if (buffer->head == NULL )
        return 0;                                           
    for ( count = 1; dummy->next != NULL ; dummy = dummy->next)
    {
        count++;
    }
    return count;
}



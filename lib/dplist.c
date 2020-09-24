#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dplist.h"

/*
 * definition of error codes
 * */
#define DPLIST_NO_ERROR 0
#define DPLIST_MEMORY_ERROR 1 // error due to mem alloc failure
#define DPLIST_INVALID_ERROR 2 //error due to a list operation applied on a NULL list

#ifdef DEBUG
    #define DEBUG_PRINTF(...)                                              \
        do {                                                     \
            fprintf(stderr,"\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);     \
            fprintf(stderr,__VA_ARGS__);                                 \
            fflush(stderr);                                                                          \
                } while(0)
#else
    #define DEBUG_PRINTF(...) (void)0
#endif


#define DPLIST_ERR_HANDLER(condition,err_code)\
    do {                                    \
            if ((condition)) DEBUG_PRINTF(#condition " failed\n");    \
            assert(!(condition));                                    \
        } while(0)

        
/*
 * The real definition of struct list / struct node
 */

struct dplist_node                                                          //每个node 内容不变
{
  dplist_node_t * prev, * next;
  void * element;
};

struct dplist                                                              //头结构发生了改变,多出来了三个function pointer
{
  dplist_node_t * head;
  void * (*element_copy)(void * src_element);
  void (*element_free)(void ** element);
  int (*element_compare)(void * x, void * y);
};


dplist_t * dpl_create (void * (*element_copy)(void * src_element),                  //比如：element_copy=&int_copy  &后边那个是全局变量区写的函数名
                       void (*element_free)(void ** element),
                       int (*element_compare)(void * x, void * y) )                 //先在main 里边写了三个函数,然后把三个函数的地址传给子函数里，子函数再传给heap里的函数指针里r
{
  dplist_t * list;                                                                  //建立一个头结构
  list = malloc(sizeof(struct dplist));
  DPLIST_ERR_HANDLER(list==NULL,DPLIST_MEMORY_ERROR);
  list->head = NULL;
  list->element_copy = element_copy;
  list->element_free = element_free;
  list->element_compare = element_compare;                                          //把main里的三个函数地址传给
  return list;
}

void dpl_free(dplist_t ** list, bool free_element)
{
  // add your code here
    dplist_node_t * dummy=(*list)->head;
    if ((*list)->head == NULL )     //也就是说这个头结构根本还没有指向任何node，直接释放头结构
    {
        free(*list);
        *list= NULL;                //每次释放heap里的地址要清空
  
    }
    else
        {
            while (dummy->next != NULL )            //判断是否到了最后一个node
                {
                    dummy = dummy->next;
                    if(free_element==true)
                    {
                        (*list)->element_free(&((dummy->prev)->element));
                    }
                    free(dummy->prev);
                    dummy->prev=NULL;
        
                }
            if(free_element==true)
                {
                    (*list)->element_free(&(dummy->element));
                }

            free(dummy);                //free 最后一个node
            dummy=NULL;
            free(*list);                //最后把头结构free
            *list=NULL;
        }

}


dplist_t * dpl_insert_at_index(dplist_t * list, void * element, int index, bool insert_copy)            //再次返回头结构的地址，其实没什么用
{                                                                                                       //element的值就是初代block的地址，记得传入之前先void cast
    dplist_node_t * ref_at_index, * list_node;
  DPLIST_ERR_HANDLER(list==NULL,DPLIST_INVALID_ERROR);
  list_node = malloc(sizeof(dplist_node_t));                //先新建一个要加的node
  DPLIST_ERR_HANDLER(list_node==NULL,DPLIST_MEMORY_ERROR);
    if(insert_copy==true)                       //把初代block的值给copy了之后指向copy的block,保留初代block给下个inset使用
    {
        list_node->element = list->element_copy(element);      //在element_copy函数里边重建一个block然后复制数值进去并且传回block的初地址(void*)
       ;
    }
    else if(insert_copy==false)                 //也就是说直接指向初代block
    {
         list_node->element = element;         // 直接传初代block,但是这样有一个问题就是a是一直指向初代的,所以下次赋值的时候就是修改这个值
    }
  // pointer drawing breakpoint
  if (list->head == NULL)               //说明还没有node,那么index也就不重要了,因为无论如何都是第0个
  { // covers case 1
    list_node->prev = NULL;
    list_node->next = NULL;
    list->head = list_node;
    // pointer drawing breakpoint
  }
  else if (index <= 0)                //说明有node了，而且要加的是第一个node前也就是新加的是首位
  { // covers case 2
    list_node->prev = NULL;
    list_node->next = list->head;
    list->head->prev = list_node;
    list->head = list_node;
    // pointer drawing breakpoint
  }
  else                                 //说明 之前有node了并且加的位数还不是首位
  {
    ref_at_index = dpl_get_reference_at_index(list, index);     //找到index处的node
    assert( ref_at_index != NULL);
    // pointer drawing breakpoint
    if (index < dpl_size(list))        //中间加
    { // covers case 4
      list_node->prev = ref_at_index->prev;             //新node的修改
      list_node->next = ref_at_index;
      ref_at_index->prev->next = list_node;             //原来index前的node的修改
      ref_at_index->prev = list_node;                   //原来index的node修改
      // pointer drawing breakpoint
    }
    else                                //末尾加
    { // covers case 3
      assert(ref_at_index->next == NULL);
      list_node->next = NULL;
      list_node->prev = ref_at_index;
      ref_at_index->next = list_node;
      // pointer drawing breakpoint
    }
  }
  return list;                  //其实这里可以不用返回修改之后的头结构,因为本身就是全局变量区的指针,已经同步修改过了
}

dplist_t * dpl_remove_at_index( dplist_t * list, int index, bool free_element)      //给一个头结构，然后进行遍历找到index node,并把这一位给移除
{                                                      //这里之所以还要判断一下就是为了决定是否要在这里把element指向的block给清除，有时候一个block是被多个node公用的,并不能删除
  // add your code here
    dplist_node_t * dummy =dpl_get_reference_at_index(list,index);
    if(dummy==NULL)                                                       //0个node
        printf("there is no more node to remove");
    else if(dummy->prev== NULL)                                           //要移除的是首位(如果移除的是首位的话就要改变头结构,其他情况下不要动头结构)
    {
            if(dummy->next==NULL)                                           //首位并且唯一一位
            {
                list->head=NULL;                                            //0个node
                if(free_element==true)                                      //如果要求顺便清楚初代block,那就顺带清除
                {
                    list->element_free(&(dummy->element));
                }
                free(dummy);                                                //如果为false则只清除node,不清除data block

            }
            else                                                        //是首位但并不是唯一一位
            {
                dummy->next->prev=NULL;
                list->head=dummy->next;
                if(free_element==true)                                      //如果要求顺便清楚初代block,那就顺带清除
                {
                    list->element_free(&(dummy->element));
                }
                free(dummy);
            }
    }
    else                                        //要移除的不是首位
    {
         if(dummy->next==NULL)                  //移除的是末位
         {
             dummy->prev->next=NULL;
             if(free_element==true)                                      //如果要求顺便清楚初代block,那就顺带清除
                {
                    list->element_free(&(dummy->element));
                }
             free(dummy);
         }
        else                                   //最正常的情况就是这种处于中间的node
        {
            dummy->prev->next=dummy->next;                       //前node的修改
            dummy->next->prev=dummy->prev;                       //后node修改
            if(free_element==true)                                      //如果要求顺便清楚初代block,那就顺带清除
                  {
                      list->element_free(&(dummy->element));
                  }
            free(dummy);
        }
        
    }
    
    return list;
}


int dpl_size( dplist_t * list )         //输入一个头结构,来找到size
{
  // add your code here
    int count;
    dplist_node_t * dummy=list->head;                        //在这里不能移动头结构里的head来找目标node,因为其他子程序也许也要用并且n用完每次都要返回,很麻烦
    DPLIST_ERR_HANDLER(list==NULL,DPLIST_INVALID_ERROR);
    if (list->head == NULL )
        return 0;                                             //也就是说这个头结构根本还没有指向任何node
    for ( count = 1; dummy->next != NULL ; dummy = dummy->next)
    {
        count++;
    }
    return count;
}


void * dpl_get_element_at_index( dplist_t * list, int index )
{
  // add your code here
    
    dplist_node_t * dummy =dpl_get_reference_at_index(list,index);
    if(dummy==NULL)
        return (void*)0;                                   //0个node的情况下返回0
    return dummy->element;
}

int dpl_get_index_of_element( dplist_t * list, void * element ) //给的是地址,比的是该地址下的数据
{
  // add your code here
    int index=0;
     dplist_node_t * dummy=list->head;
    if (dummy == NULL )                             //0 node
    {
        printf("there is no node");
        return 0;
    }
    for (int count = 0; dummy->next != NULL ; dummy = dummy->next, count++,index=count)
    {
        if(list->element_compare(element,dummy->element)==0)  //二者地址下的值相同则返回0
      {
          return count;
      }
        
    }
    if(list->element_compare(element,dummy->element)==0)           //最后一位无法在循环体内j实现,因此单独分出来
         {
             return index;
         }
    else
    {
        printf("this element does not exist\n");
        return -1;
    }
}

dplist_node_t * dpl_get_reference_at_index( dplist_t * list, int index )

{
  int count;
  dplist_node_t * dummy;                        //在这里不能移动头结构里的head来找目标node,因为其他子程序也许也要用并且n用完每次都要返回,很麻烦
  DPLIST_ERR_HANDLER(list==NULL,DPLIST_INVALID_ERROR);
  if (list->head == NULL )
      return NULL;                                             //也就是说这个头结构根本还没有指向任何node
  for ( dummy = list->head, count = 0; dummy->next != NULL ; dummy = dummy->next, count++)
  {
    if (count >= index)
        return dummy;
  }
  return dummy;                       //这个程序有一个问题就是没有提前判断index和size的关系,因此极有可能遍历到最后一位返回最后一位which is not locate at index
}
 
void * dpl_get_element_at_reference( dplist_t * list, dplist_node_t * reference ) //之前的是给index来找elements,这个是给node的地址来找element
{
    // add your code here
    dplist_node_t * dummy;
    if (list->head == NULL )
        return NULL;                    //空的list,返回NULL
    if(reference==NULL)        //该node的element还没有指向
    {
        int size =dpl_size(list);
        void * last;
        last= dpl_get_reference_at_index(list,size-1)->element ;
        return last;
    }
    for(dummy=list->head;dummy->next !=NULL;dummy=dummy->next)        //遍历list来找有没有相同地址的node
     {
         if(dummy==reference)               //先去判断reference的node存在然后再去输出element
         {
             return reference->element;
             
         }
     }
    if(dummy==reference)               //判断最后一位node
    {
        return reference->element;
        
    }
     return NULL;
        
    
}
void* data_element_copy(void * element)
{
    element_t* new = NULL;
    new = malloc(sizeof(element_t));
    *new = *(element_t *)element;            //给new block一个相同值
    return ((void*)new);
}

void data_element_free(void **element)       //element的值是一个node里element的地址
{
    element_t* tag;
    tag=(element_t*)(*element);
    free(tag);
    tag=NULL;
}

int data_element_compare(void * x, void * y)        // 给了两个地址,最后是比两个地址下的isensor_id是否相同
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






#ifndef __errmacros_h__
#define __errmacros_h__

#include <errno.h>

#define SYSCALL_ERROR(err)                                     \
        do {                                                \
            if ( (err) == -1 )                                \
            {                                                \
                perror("Error executing syscall");            \
                exit( EXIT_FAILURE );                        \
            }                                                \
        } while(0)
        
#define CHECK_MKFIFO(err)                                     \
        do {                                                \
            if ( (err) == -1 )                                \
            {                                                \
                if ( errno != EEXIST )                        \
                {                                            \
                    perror("Error executing mkfifo");        \
                    exit( EXIT_FAILURE );                    \
                }                                            \
            }                                                \
        } while(0)
#define CHECK_PIPE(err)                                     \
do {                                                \
    if ( (err) == -1 )                                \
    {                                                \
        if ( errno != EEXIST )                        \
        {                                            \
            perror("Error create pipe");        \
            exit( EXIT_FAILURE );                    \
        }                                            \
    }                                                \
} while(0)
#define CHECK_MUTEX_LOCK(err)                                     \
do {                                                \
    if ( (err) !=0 )                                \
    {                                                \
                                                   \
            perror("pthread_mutex_lock failed");        \
            exit( EXIT_FAILURE );                    \
        }                                            \
                                                    \
} while(0)
#define CHECK_MUTEX_UNLOCK(err)                                     \
do {                                                \
    if ( (err) !=0 )                                \
    {                                                \
                               \
                                                    \
            perror("pthread_mutex_unlock failed");        \
            exit( EXIT_FAILURE );                    \
                                                    \
    }                                                \
} while(0)
        
#define FILE_OPEN_ERROR(fp)                                 \
        do {                                                \
            if ( (fp) == NULL )                                \
            {                                                \
                perror("File open failed");                    \
                exit( EXIT_FAILURE );                        \
            }                                                \
        } while(0)

#define FILE_CLOSE_ERROR(err)                                 \
        do {                                                \
            if ( (err) == -1 )                                \
            {                                                \
                perror("File close failed");                \
                exit( EXIT_FAILURE );                        \
            }                                                \
        } while(0)

#define ASPRINTF_ERROR(err)                                 \
        do {                                                \
            if ( (err) == -1 )                                \
            {                                                \
                perror("asprintf failed");                    \
                exit( EXIT_FAILURE );                        \
            }                                                \
        } while(0)
#define SBUFFER_ERROR(err)                                  \
do {                                                        \
    if ( (err) == -1 )                                      \
    {                                                       \
        perror("sbuffer create failed");                    \
        exit( EXIT_FAILURE );                               \
    }                                                       \
} while(0)
#define SBUFFER_FREE_ERROR(err)                             \
do {                                                        \
    if ( (err) == -1 )                                      \
    {                                                       \
        perror("sbuffer free failed");                      \
        exit( EXIT_FAILURE );                               \
    }                                                       \
} while(0)
#define PTHREAD_CONNMGR_CREATE(err)                         \
do {                                                        \
    if ( (err) == -1 )                                      \
    {                                                       \
        perror("thread_connmgr created failed\n");          \
        exit( EXIT_FAILURE );                               \
    }                                                       \
    if ( (err) == 0 )                                       \
    {                                                       \
    printf("thread_connmgr created successfully\n");         \
    }                                                       \
} while(0)

#define PTHREAD_DATAMGR_CREATE(err)                         \
do {                                                        \
    if ( (err) == -1 )                                      \
    {                                                       \
        perror("thread_datamgr created failed\n");                      \
        exit( EXIT_FAILURE );                               \
    }                                                       \
if ( (err) == 0 )                                           \
   {                                                         \
   printf("thread_datamgr created successfully\n");         \
   }                                                        \
} while(0)

#define PTHREAD_SQL_CREATE(err)                             \
do {                                                        \
    if ( (err) == -1 )                                      \
    {                                                       \
        perror("thread_sql created failed\n");                      \
        exit( EXIT_FAILURE );                               \
    }                                                       \
    if ( (err) == 0 )                                           \
    {                                                         \
    printf("thread_sql created successfully\n");         \
    }                                                        \
} while(0)
#define PTHREAD_DATAMGR_CLOSE(err)                         \
do {                                                        \
    if ( (err) == -1 )                                      \
    {                                                       \
        perror("thread_datamgr close failed\n");                      \
        exit( EXIT_FAILURE );                               \
    }                                                       \
if ( (err) == 0 )                                           \
   {                                                         \
   printf("thread_datamgr closed successfully\n");         \
   }                                                        \
} while(0)
#define PTHREAD_CONNMGR_CLOSE(err)                         \
do {                                                        \
    if ( (err) == -1 )                                      \
    {                                                       \
        perror("thread_connmgr close failed\n");                      \
        exit( EXIT_FAILURE );                               \
    }                                                       \
if ( (err) == 0 )                                           \
   {                                                         \
   printf("thread_connmgr closed successfully\n");         \
   }                                                        \
} while(0)
#define PTHREAD_SQL_CLOSE(err)                         \
do {                                                        \
    if ( (err) == -1 )                                      \
    {                                                       \
        perror("thread_sql close failed\n");                      \
        exit( EXIT_FAILURE );                               \
    }                                                       \
if ( (err) == 0 )                                           \
   {                                                         \
   printf("thread_sql closed successfully\n");         \
   }                                                        \
} while(0)

#define PTHREAD_MUTEX_DESTROY(err)                         \
do {                                                        \
    if ( (err) == -1 )                                      \
    {                                                       \
        perror("PTHREAD_MUTEX_DESTROY failed\n");                      \
        exit( EXIT_FAILURE );                               \
    }                                                       \
if ( (err) == 0 )                                           \
   {                                                         \
   printf("PTHREAD_MUTEX_DESTROY successfully\n");         \
   }                                                        \
} while(0)

#define FFLUSH_ERROR(err)                                     \
        do {                                                \
            if ( (err) == EOF )                                \
            {                                                \
                perror("fflush failed");                    \
                exit( EXIT_FAILURE );                        \
            }                                                \
        } while(0)

#endif



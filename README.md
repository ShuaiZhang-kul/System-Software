# System-Software
This project is a "Operating System-like" software which running one parent process and one child process based on Linux kernal written in C language.
For the parent process, it contains three threads made by：
1.connmgr.c which is a manager for the TCP connection for different sensors. And it can assign sockets for each sensor and clear "dead" socket, Epoll API is used inside.
2.datamgr.c which is a manager for detecting the sensor data sent to the Sbuffer, if some of the node have a higher or lower temperature, it will send signal to the child process
and also for storing the data into the sbuffer created in heap memory and manege the memory based on dplist structure which is in lib folder.
3.sensordb.c which is a manager used to read data from the sbuffer and store it into the sql database.
For the child process, FIFO pipe is used to transmit the String data from parent process to child process and record abnormal situation(abnormal temperature) in the log file.
And a normal pipe is used to inform child process if the parent process is still running to terminating itself.
For the dplist.c it generate a data structure to store the data for different sensors at different time. Each node store the data from one sensor at a specific time.
To Avoid the chao access to the sbuffer from three main thread, Pthread_lock is used for managing the usage of CPU.
In summary, this software has parts of the function of operating system like memory management, I/O management, CPU management, TCP-management and thread&process management.
GCC is used for the compiling and valgrind is used for the memory leak detection.
Furthermore, working principal.jpg, terminating of software.jpg and memory overview.jpg can be found in the folder to illustrate this project.



#ifndef _MEMDEV_H_
#define _MEMDEV_H_

#ifndef MEMDEV_MAJOR
#define MEMDEV_MAJOR 0  /*Ô¤ÉèµÄmemµÄÖ÷Éè±¸ºÅ*/
#endif

#ifndef MEMDEV_NR_DEVS
#define MEMDEV_NR_DEVS 2    /*For the share memory, need at least 2 */
#endif

#ifndef MEMDEV_SIZE
#define MEMDEV_SIZE 4096
#endif

#ifndef _KERNEL_
#define _KERNEL_
#endif

/*memÉè±¸ÃèÊö½á¹¹Ìå*/
struct mem_dev                                     
{                                                        
  char *data;                      
  unsigned long size;    
  struct semaphore sem;  
};

#endif /* _MEMDEV_H_ */

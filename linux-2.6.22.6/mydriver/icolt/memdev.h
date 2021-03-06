#ifndef _MEMDEV_H_
#define _MEMDEV_H_

#include <linux/ioctl.h>

#ifndef MEMDEV_MAJOR
#define MEMDEV_MAJOR 0   /*major device number*/
#endif

#ifndef MEMDEV_NR_DEVS
#define MEMDEV_NR_DEVS 2    /*device quantity*/
#endif

#ifndef MEMDEV_SIZE
#define MEMDEV_SIZE 4096
#endif


struct mem_dev                                     
{                                                        
  char *data;                      
  unsigned long size;       
};

/* the type of the command */
#define MEMDEV_IOC_MAGIC  'k'

/*define the command*/
#define MEMDEV_IOCPRINT   _IO(MEMDEV_IOC_MAGIC, 1)
#define MEMDEV_IOCGETDATA _IOR(MEMDEV_IOC_MAGIC, 2, int)
#define MEMDEV_IOCSETDATA _IOW(MEMDEV_IOC_MAGIC, 3, int)

#define MEMDEV_IOC_MAXNR 3

#endif /* _MEMDEV_H_ */

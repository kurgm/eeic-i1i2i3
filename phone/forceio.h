#ifndef FORCEIO_H_
#define FORCEIO_H_

#include <sys/types.h>

int write_force(int fildes, const void *ptr, size_t nbyte);

int send_force(int socket, const void *buffer, size_t length, int flags);

#endif /* FORCEIO_H_ */

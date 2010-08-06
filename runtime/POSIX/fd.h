/*
 * fd.h
 *
 *  Created on: Aug 6, 2010
 *      Author: stefan
 */

#ifndef FD_H_
#define FD_H_

#define FD_IS_CONCRETE      (1 << 0)
#define FD_CLOSE_ON_EXEC    (1 << 6)
#define FD_CAN_READ         (1 << 1)
#define FD_CAN_WRITE        (1 << 2)
#define FD_IS_FILE          (1 << 3)
#define FD_IS_SOCKET        (1 << 4)
#define FD_IS_PIPE          (1 << 5)

typedef struct {
  unsigned int flags;

  int concrete_fd;

  void *io_object;

  char allocated;
} fd_entry_t;


#endif /* FD_H_ */

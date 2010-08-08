/*
 * buffers.h
 *
 *  Created on: Aug 6, 2010
 *      Author: stefan
 */

#ifndef BUFFERS_H_
#define BUFFERS_H_

#include <unistd.h>

#include "common.h"
#include "multiprocess.h"

#define EVENT_READ  (1 << 0)
#define EVENT_WRITE (1 << 1)
#define EVENT_ERROR (1 << 2)

typedef struct {
  char events;
  wlist_id_t wlist;

  char allocated;
} buffer_event_t;

typedef struct {
  char *contents;
  size_t max_size;

  off_t start;
  size_t size;

  buffer_event_t evt_queue[MAX_EVENTS];
  wlist_id_t empty_wlist;
  wlist_id_t full_wlist;
} stream_buffer_t;

void _stream_init(stream_buffer_t *buff, size_t max_size);
void _stream_destroy(stream_buffer_t *buff);
ssize_t _stream_read(stream_buffer_t *buff, char *dest, size_t count);
ssize_t _stream_write(stream_buffer_t *buff, char *src, size_t count);
int _stream_register_event(stream_buffer_t *buff, char events, wlist_id_t wlist);
int _stream_clear_event(stream_buffer_t *buff, wlist_id_t wlist);


typedef struct {
  char *contents;
  size_t max_size;
} block_buffer_t;


void _block_init(block_buffer_t *buff, size_t max_size);
void _block_destroy(block_buffer_t *buff);
ssize_t _block_read(block_buffer_t *buff, char *dest, size_t count, off_t offset);
ssize_t _block_write(block_buffer_t *buff, char *src, size_t count, off_t offset);


#endif /* BUFFERS_H_ */

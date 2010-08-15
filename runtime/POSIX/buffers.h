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

// A basic producer-consumer data structure
typedef struct {
  char *contents;
  size_t max_size;

  size_t start;
  size_t size;

  wlist_id_t evt_queue[MAX_EVENTS];
  wlist_id_t empty_wlist;
  wlist_id_t full_wlist;

  unsigned int queued;
  char destroying;
  char closed;
} stream_buffer_t;

stream_buffer_t *_stream_create(size_t max_size);
void _stream_destroy(stream_buffer_t *buff);

ssize_t _stream_read(stream_buffer_t *buff, char *dest, size_t count);
ssize_t _stream_write(stream_buffer_t *buff, const char *src, size_t count);
void _stream_close(stream_buffer_t *buff);

int _stream_register_event(stream_buffer_t *buff, wlist_id_t wlist);
int _stream_clear_event(stream_buffer_t *buff, wlist_id_t wlist);

static inline int _stream_is_empty(stream_buffer_t *buff) {
  return (buff->size == 0);
}

static inline int _stream_is_full(stream_buffer_t *buff) {
  return (buff->size == buff->max_size);
}

typedef struct {
  char *contents;
  size_t max_size;
  size_t size;
} block_buffer_t;


void _block_init(block_buffer_t *buff, size_t max_size);
void _block_finalize(block_buffer_t *buff);
ssize_t _block_read(block_buffer_t *buff, char *dest, size_t count, size_t offset);
ssize_t _block_write(block_buffer_t *buff, const char *src, size_t count, size_t offset);


#endif /* BUFFERS_H_ */

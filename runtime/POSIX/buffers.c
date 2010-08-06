/*
 * buffers.c
 *
 *  Created on: Aug 6, 2010
 *      Author: stefan
 */

#include "buffers.h"

#include "lists.h"

#include <stdlib.h>
#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// Stream Buffers
////////////////////////////////////////////////////////////////////////////////

void __notify_event(stream_buffer_t *buff, char event) {
  if (event & EVENT_READ)
    klee_thread_notify_all(buff->empty_wlist);

  if (event & EVENT_WRITE)
    klee_thread_notify_all(buff->full_wlist);

  unsigned int i;
  for (i = 0; i < MAX_EVENTS; i++) {
    if (!STATIC_LIST_CHECK(buff->evt_queue, i))
      continue;
    buffer_event_t *evt = &buff->evt_queue[i];

    if (evt->events & event) {
      klee_thread_notify_all(evt->wlist);
      STATIC_LIST_CLEAR(buff->evt_queue, i);
    }
  }
}

void _stream_init(stream_buffer_t *buff, size_t max_size) {
  memset(buff, 0, sizeof(stream_buffer_t));
  buff->contents = (char*) malloc(max_size);
  buff->max_size = max_size;
  STATIC_LIST_INIT(buff->evt_queue);
}

void _stream_destroy(stream_buffer_t *buff) {
  free(buff->contents);
}

ssize_t _stream_read(stream_buffer_t *buff, char *dest, size_t count) {
  if (count == 0)
    return 0;

  while (buff->size == 0)
    klee_thread_sleep(buff->empty_wlist);

  if (buff->size < count)
    count = buff->size;

  if (buff->start + count > buff->max_size) {
    size_t overflow = (buff->start + count) % buff->max_size;

    memcpy(dest, &buff->contents[buff->start], count - overflow);
    memcpy(&dest[count-overflow], &buff->contents[0], overflow);
  } else {
    memcpy(dest, &buff->contents[buff->start], count);
  }

  buff->start = (buff->start + count) % buff->max_size;
  buff->size -= count;

  __notify_event(buff, EVENT_WRITE);

  return count;
}

ssize_t _stream_write(stream_buffer_t *buff, char *src, size_t count) {
  if (count == 0)
    return 0;

  while (buff->size == buff->max_size)
    klee_thread_sleep(buff->full_wlist);

  if (count > buff->max_size - buff->size)
    count = buff->max_size - buff->size;

  size_t end = (buff->start + buff->size) % buff->max_size;

  if (end + count > buff->max_size) {
    size_t overflow = (end + count) % buff->max_size;

    memcpy(&buff->contents[end], src, count - overflow);
    memcpy(&buff->contents[0], &src[count - overflow], overflow);
  } else {
    memcpy(&buff->contents[end], src, count);
  }

  buff->size += count;

  __notify_event(buff, EVENT_READ);

  return count;
}

int _stream_register_event(stream_buffer_t *buff, char events, wlist_id_t wlist) {
  unsigned int idx;
  STATIC_LIST_ALLOC(buff->evt_queue, idx);

  if (idx == MAX_EVENTS)
    return -1;

  buff->evt_queue[idx].events = events;
  buff->evt_queue[idx].wlist = wlist;

  return 0;
}

int _stream_clear_event(stream_buffer_t *buff, wlist_id_t wlist) {
  unsigned int idx;
  for (idx = 0; idx < MAX_EVENTS; idx++) {
    if (!STATIC_LIST_CHECK(buff->evt_queue, idx))
      continue;

    if (buff->evt_queue[idx].wlist == wlist) {
      STATIC_LIST_CLEAR(buff->evt_queue, idx);
      return 0;
    }
  }

  return -1;
}

////////////////////////////////////////////////////////////////////////////////
// Block buffers
////////////////////////////////////////////////////////////////////////////////

void _block_init(block_buffer_t *buff, size_t max_size) {
  memset(buff, 0, sizeof(block_buffer_t));
  buff->contents = (char*)malloc(max_size);
  buff->max_size = max_size;
}

void _block_destroy(block_buffer_t *buff) {
  free(buff->contents);
}

ssize_t _block_read(block_buffer_t *buff, char *dest, size_t count, off_t offset) {
  if (offset + count > buff->max_size)
    count = buff->max_size - offset;

  if (count == 0)
    return 0;

  memcpy(dest, &buff->contents[offset], count);

  return count;
}

ssize_t _block_write(block_buffer_t *buff, char *src, size_t count, off_t offset) {
  if (offset + count > buff->max_size)
    count = buff->max_size - offset;

  if (count == 0)
    return 0;

  memcpy(&buff->contents[offset], src, count);

  return count;
}

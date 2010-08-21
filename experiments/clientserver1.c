/*
 * clientserver1.c
 * A producer-consumer implementation, meant to test basic Cloud9 load
 * balancing functionality and bug detection capabilities
 *
 * This version should behave *correctly* under any scheduling decisions
 *
 *  Created on: Aug 21, 2010
 *      Author: stefan
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <pthread.h>

#define QUEUE_SIZE          3
#define PRODUCERS_COUNT     5
#define CONSUMERS_COUNT     5
#define UNITS_PER_PRODUCER  2

#define LISTEN_PORT     41788
#define BACKLOG_SIZE    (PRODUCERS_COUNT * 2)

typedef struct {
  pthread_mutex_t   mutex;
  pthread_cond_t    empty;
  pthread_cond_t    full;

  int               *data;

  unsigned int      start;
  unsigned int      end;

  unsigned int      size;
  unsigned int      capacity;

  int closed;
} queue_t;

typedef struct {
  queue_t *q;

  int sockfd;
  int closefd;
} producer_data_t;

static void queue_init(queue_t *q, unsigned int capacity) {
  memset(q, 0, sizeof(queue_t));

  q->data = (int*)malloc(capacity*sizeof(int));
  q->capacity = capacity;

  q->size = 0;
  q->start = 0;
  q->end = 0;

  q->closed = 0;

  pthread_mutex_init(&q->mutex, NULL);
  pthread_cond_init(&q->empty, NULL);
  pthread_cond_init(&q->full, NULL);
}

static void queue_produce(queue_t *q, int c) {
  pthread_mutex_lock(&q->mutex);

  while (q->size == q->capacity) {
    pthread_cond_wait(&q->full, &q->mutex);
  }

  assert(!q->closed);

  q->data[q->end] = c;
  q->end = (q->end + 1) % q->capacity;
  q->size = q->size + 1;

  pthread_cond_broadcast(&q->empty);

  pthread_mutex_unlock(&q->mutex);
}

static int queue_consume(queue_t *q) {
  pthread_mutex_lock(&q->mutex);

  while (q->size == 0) {
    if (q->closed) {
      pthread_mutex_unlock(&q->mutex);
      return -1;
    }

    pthread_cond_wait(&q->empty, &q->mutex);
  }

  int result = q->data[q->start];
  q->start = (q->start + 1) % q->capacity;
  q->size = q->size - 1;

  pthread_cond_broadcast(&q->full);

  pthread_mutex_unlock(&q->mutex);

  return result;
}

static void queue_close(queue_t *q) {
  pthread_mutex_lock(&q->mutex);

  q->closed = 1;

  pthread_cond_broadcast(&q->empty);

  pthread_mutex_unlock(&q->mutex);
}

static void queue_deinit(queue_t *q) {
  pthread_mutex_destroy(&q->mutex);
  pthread_cond_destroy(&q->empty);
  pthread_cond_destroy(&q->full);

  free(q->data);
}

void *consumer_thread(void *data) {
  queue_t *q = (queue_t*)data;

  int c;

  while ((c = queue_consume(q)) >= 0) {
    printf("Consumed data: %d\n", c);
  }

  printf("Consumer thread terminated\n");

  return NULL;
}

void *producer_thread(void *data) {
  producer_data_t *pdata = (producer_data_t*)data;

  int c;

  ssize_t res = read(pdata->sockfd, &c, sizeof(c));
  assert(res == sizeof(c));

  c = ntohl((uint32_t)c);

  if (c < 0) {
    queue_close(pdata->q);
    close(pdata->closefd);
  } else {
    queue_produce(pdata->q, c);
  }

  close(pdata->sockfd);
  free(pdata);

  return NULL;
}

void serve(queue_t *q, int accsock) {
  int res;

  // Create the close notification pipe
  int closepipe[2];
  res = pipe(closepipe);
  assert(res == 0);

  fd_set fdset;
  int nfds = accsock;
  if (closepipe[0] > nfds) {
    nfds = closepipe[0];
  }
  nfds++;

  for (;;) {
    FD_ZERO(&fdset);
    FD_SET(accsock, &fdset);
    FD_SET(closepipe[0], &fdset);

    res = select(nfds, &fdset, NULL, NULL, NULL);
    assert(res > 0);

    if (FD_ISSET(closepipe[0], &fdset)) {
      int buf;
      res = read(closepipe[0], &buf, sizeof(buf));
      assert(res == 0);

      close(closepipe[0]);
      break;
    }

    if (FD_ISSET(accsock, &fdset)) {
      // Accept all connections until we have EAGAIN
      for (;;) {
        int connsock = accept(accsock, NULL, NULL);
        if (connsock == -1) {
          assert(errno = EAGAIN);
          break;
        }

        // Create a new thread to serve this connection
        producer_data_t *pdata = (producer_data_t*)malloc(sizeof(*pdata));
        memset(pdata, 0, sizeof(*pdata));
        pdata->q = q;
        pdata->closefd = closepipe[1];
        pdata->sockfd = connsock;

        pthread_t prodthread;
        pthread_create(&prodthread, NULL, &producer_thread, pdata);
        pthread_detach(prodthread); // Don't want to join all producers
      }
    }

    // Now go back and wait for another event
  }

  close(accsock);
}

int server(int initfd) {
  // Create the shared queue
  queue_t q;
  int res;

  queue_init(&q, QUEUE_SIZE);

  // Create the consumer threads
  pthread_t cthreads[CONSUMERS_COUNT];
  unsigned i;
  for (i = 0; i < CONSUMERS_COUNT; i++) {
    pthread_create(&cthreads[i], NULL, &consumer_thread, &q);
  }

  // Create the listening socket
  int accsock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  assert(accsock >= 0);

  int true = 1;
  res = setsockopt(accsock, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(true));
  assert(res == 0);

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(LISTEN_PORT);

  res = bind(accsock, (struct sockaddr*)&addr, sizeof(addr));
  assert(res == 0);

  res = listen(accsock, BACKLOG_SIZE);
  assert(res == 0);

  close(initfd);

  printf("Server initialized\n");

  serve(&q, accsock);

  for (i = 0; i < CONSUMERS_COUNT; i++) {
    pthread_join(cthreads[i], NULL);
  }

  queue_deinit(&q);
  return 0;
}

void send_unit(int value) {
  //if (value >= 0)
  //  printf("Created data: %d\n", value);

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  assert(sock >= 0);

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(LISTEN_PORT);

  int res = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
  assert(res == 0);

  value = htonl((uint32_t)value);
  ssize_t count = write(sock, &value, sizeof(value));
  assert(count == sizeof(value));

  shutdown(sock, SHUT_WR);

  int buf;
  count = read(sock, &buf, sizeof(buf));
  assert(count == 0);

  close(sock);
}

void send_units(int start, int count) {
  int i;
  for (i = start; i < start + count; i++) {
    send_unit(i);
  }
}

int client(void) {
  int forks = 0;

  unsigned i;
  for (i = 0; i < PRODUCERS_COUNT; i++) {
    pid_t pid = fork();
    assert(pid >= 0);

    if (pid == 0) {
      send_units(UNITS_PER_PRODUCER*i, UNITS_PER_PRODUCER);
      return 0;
    } else {
      forks++;
    }
  }

  while (forks > 0) {
    int res = wait(NULL);
    assert(res != -1);
    forks--;
  }

  send_unit(-1);

  return 0;
}

int main(int argc, char **argv) {
  int res;

  int initpipe[2];
  res = pipe(initpipe);
  assert(res == 0);

  pid_t pid = fork();
  assert(pid >= 0);

  if (pid == 0) {
    close(initpipe[1]);

    int buf;
    ssize_t count = read(initpipe[0], &buf, sizeof(buf));
    assert(count == 0);
    close(initpipe[0]);

    return client();
  }

  close(initpipe[0]);
  res = server(initpipe[1]);

  pid_t wpid = wait(NULL);
  assert(wpid = pid);

  return res;
}

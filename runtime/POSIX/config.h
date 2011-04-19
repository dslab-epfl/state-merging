/*
 * config.h - POSIX Model Configuration
 *
 *  Created on: Oct 3, 2010
 *      Author: stefan
 */

#ifndef POSIX_CONFIG_H_
#define POSIX_CONFIG_H_

////////////////////////////////////////////////////////////////////////////////
// System Limits
////////////////////////////////////////////////////////////////////////////////

#define MAX_THREADS         16
#define MAX_PROCESSES       8

#define MAX_SEMAPHORES      8

#define MAX_EVENTS          4

#define MAX_FDS             64
#define MAX_FILES           16

#define MAX_PATH_LEN        75

#define MAX_PORTS           32
#define MAX_UNIX_EPOINTS    32

#define MAX_PENDING_CONN    4

#define MAX_MMAPS           4

#define MAX_STDINSIZE       16

#define MAX_DGRAM_SIZE          65536
#define MAX_NUMBER_DGRAMS       8
#define STREAM_BUFFER_SIZE      4096
#define PIPE_BUFFER_SIZE        4096
#define SENDFILE_BUFFER_SIZE    256

////////////////////////////////////////////////////////////////////////////////
// Enabled Components
////////////////////////////////////////////////////////////////////////////////

#define HAVE_FAULT_INJECTION    1
//#define HAVE_SYMBOLIC_CTYPE     1


#endif /* POSIX_CONFIG_H_ */

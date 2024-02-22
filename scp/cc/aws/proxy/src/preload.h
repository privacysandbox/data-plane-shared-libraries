// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef AWS_PROXY_PRELOAD_H_
#define AWS_PROXY_PRELOAD_H_

#include <arpa/nameser.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <resolv.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <linux/vm_sockets.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

// Define possible interfaces with C linkage so that all the signatures and
// interfaces are consistent with libc.
extern "C" {
// This is called when the whole preload library is loaded to the app program.
static void preload_init(void) __attribute__((constructor));

// Pointer to libc functions
static int (*libc_connect)(int sockfd, const struct sockaddr* addr,
                           socklen_t addrlen);
static int (*libc_close)(int fd);
static int (*libc_res_ninit)(res_state statep);
static int (*libc_res_init)(void);
static int (*libc_setsockopt)(int sockfd, int level, int optname,
                              const void* optval, socklen_t optlen);
static int (*libc_getsockopt)(int sockfd, int level, int optname,
                              void* __restrict optval,
                              socklen_t* __restrict optlen);
static int (*libc_ioctl)(int fd, uint32_t request, ...);
static int (*libc_bind)(int sockfd, const struct sockaddr* addr,
                        socklen_t addrlen);
static int (*libc_listen)(int sockfd, int backlog);
static int (*libc_accept)(int sockfd, struct sockaddr* addr,
                          socklen_t* addrlen);
static int (*libc_accept4)(int sockfd, struct sockaddr* addr,
                           socklen_t* addrlen, int flags);
static int (*libc_epoll_ctl)(int epfd, int op, int fd,
                             struct epoll_event* event);
// The ioctl() syscall signature contains variadic arguments for historical
// reasons (i.e. allowing different types without forced casting). However, a
// real syscall cannot have variadic arguments at all. The real internal
// signature is really just an argument with char* or void* type.
// ref: https://static.lwn.net/images/pdf/LDD3/ch06.pdf
int ioctl(int fd, uint32_t request, void* argp);
}

static int socks5_client_connect(int sockfd, const struct sockaddr* addr);

void preload_init(void);

#define EXPORT __attribute__((visibility("default")))

EXPORT int res_init(void);

EXPORT int res_ninit(res_state statep);

EXPORT int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);

EXPORT int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);

EXPORT int setsockopt(int sockfd, int level, int optname, const void* optval,
                      socklen_t optlen);

EXPORT int getsockopt(int sockfd, int level, int optname,
                      void* __restrict optval, socklen_t* __restrict optlen);

// A wrapper for resuming recv() call on EINTR.
// Return the total number of bytes received.
static ssize_t recv_all(int fd, void* buf, size_t len, int flags);

// In java applications, at the end of plaintext connections (e.g. HTTP), java
// may call ioctl(FIONREAD) to get to know if there is any remaining data to
// consume. FIONREAD isn't supported on VSOCK, so we just fake it. To make sure
// most ioctl calls still follow the fastest path, here we still make the ioctl
// syscall first, and only on errors we kick-in and check if that's the case we
// want to handle.
EXPORT int ioctl(int fd, uint32_t request, void* argp);

int socks5_client_connect(int sockfd, const struct sockaddr* addr);

EXPORT int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);

EXPORT int listen(int sockfd, int backlog);

EXPORT int accept4(int sockfd, struct sockaddr* addr, socklen_t* addrlen,
                   int flags);

EXPORT int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);

#endif  // AWS_PROXY_PRELOAD_H_

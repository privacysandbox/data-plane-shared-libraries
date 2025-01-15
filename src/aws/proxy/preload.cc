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

#include "preload.h"

#include <errno.h>
#include <sys/types.h>

#include <algorithm>
#include <cstdint>

#include "protocol.h"
#include "socket_vendor_protocol.h"

namespace socket_vendor = google::scp::proxy::socket_vendor;

static int socks5_client_connect(int sockfd, const struct sockaddr* addr);

namespace {
class AutoCloseFd {
 public:
  explicit AutoCloseFd(int fd) : fd_(fd) {}

  ~AutoCloseFd() {
    if (libc_close) {
      libc_close(fd_);
    } else {
      close(fd_);
    }
  }

  int get() const { return fd_; }

 private:
  int fd_;
};

// Convert the file descriptor sockfd into a VSOCK socket. This means atomically
// closing sockfd and create a new VSOCK socket descriptor of the same value.
// Returns the fcntl flags of the original sockfd.
int ConvertToVsock(int sockfd) {
  int vsock_fd = socket(AF_VSOCK, SOCK_STREAM, 0);
  if (vsock_fd < 0) {
    return -1;
  }
  AutoCloseFd autoclose(vsock_fd);
  int flags = fcntl(sockfd, F_GETFL);
  if (dup2(vsock_fd, sockfd) < 0) {
    return -1;
  }
  return flags;
}

}  // namespace

void preload_init(void) {
// Some functions may be actually a macro. Here these two macros will stringize
// the argument, so that we can use STR(func_name) to refer the real symbol name
// as string.
#define _STR(s) #s
#define STR(s) _STR(s)
  libc_connect =
      reinterpret_cast<decltype(libc_connect)>(dlsym(RTLD_NEXT, STR(connect)));
  libc_close =
      reinterpret_cast<decltype(libc_close)>(dlsym(RTLD_NEXT, STR(close)));
  libc_res_init = reinterpret_cast<decltype(libc_res_init)>(
      dlsym(RTLD_NEXT, STR(res_init)));
  libc_res_ninit = reinterpret_cast<decltype(libc_res_ninit)>(
      dlsym(RTLD_NEXT, STR(res_ninit)));
  libc_setsockopt = reinterpret_cast<decltype(libc_setsockopt)>(
      dlsym(RTLD_NEXT, STR(setsockopt)));
  libc_getsockopt = reinterpret_cast<decltype(libc_getsockopt)>(
      dlsym(RTLD_NEXT, STR(getsockopt)));
  libc_ioctl =
      reinterpret_cast<decltype(libc_ioctl)>(dlsym(RTLD_NEXT, STR(ioctl)));
  libc_bind =
      reinterpret_cast<decltype(libc_bind)>(dlsym(RTLD_NEXT, STR(bind)));
  libc_listen =
      reinterpret_cast<decltype(libc_listen)>(dlsym(RTLD_NEXT, STR(listen)));
  libc_accept =
      reinterpret_cast<decltype(libc_accept)>(dlsym(RTLD_NEXT, STR(accept)));
  libc_accept4 =
      reinterpret_cast<decltype(libc_accept4)>(dlsym(RTLD_NEXT, STR(accept4)));
  libc_epoll_ctl = reinterpret_cast<decltype(libc_epoll_ctl)>(
      dlsym(RTLD_NEXT, STR(epoll_ctl)));
#undef _STR
#undef STR
}

#define EXPORT __attribute__((visibility("default")))

EXPORT int res_init(void) throw() {
  if (libc_res_init == nullptr) {
    return -1;
  }
  int r = libc_res_init();
  // Force DNS lookups to use TCP, as we don't support UDP-based socks5 proxying
  // yet.
  _res.options |= RES_USEVC;
  return r;
}

EXPORT int res_ninit(res_state statep) throw() {
  int r = libc_res_ninit(statep);
  statep->options |= RES_USEVC;
  return r;
}

EXPORT int epoll_ctl(int epfd, int op, int fd,
                     struct epoll_event* event) throw() {
  // The reason why we need to hook epoll_ctl is that, certain applications,
  // such as boost::asio, may add the socket into an epoll instance before
  // connect(). This is trouble some, because we need to make that a VSOCK
  // socket, and it does not happen automatically in the epoll instance. We'll
  // essentially need to remove the old fd, convert to VSOCK, and add to epoll
  // again with the same epoll_event. This would require us to remember
  // epoll_events. To avoid doing that, we convert the socket before adding to
  // the epoll instance.
  // Fallback early if it is not EPOLL_CTL_ADD.
  if (op != EPOLL_CTL_ADD) {
    return libc_epoll_ctl(epfd, op, fd, event);
  }
  // Get the socket type and domain.
  int sock_type = 0;
  socklen_t sock_type_len = sizeof(sock_type);
  int ret = libc_getsockopt(fd, SOL_SOCKET, SO_TYPE,
                            static_cast<void*>(&sock_type), &sock_type_len);
  int sock_domain = 0;
  socklen_t sock_domain_len = sizeof(sock_domain);
  // We need two libc_getsockopt() calls, one for SO_TYPE, one for SO_DOMAIN. If
  // either fails, fallback to libc_epoll_ctl.
  if (ret != 0 || libc_getsockopt(fd, SOL_SOCKET, SO_DOMAIN,
                                  static_cast<void*>(&sock_domain),
                                  &sock_domain_len) != 0) {
    return libc_epoll_ctl(epfd, op, fd, event);
  }
  // We only care about IP/IPv6 TCP sockets. Fallback otherwise.
  if (sock_type != SOCK_STREAM ||
      (sock_domain != AF_INET && sock_domain != AF_INET6)) {
    return libc_epoll_ctl(epfd, op, fd, event);
  }
  // If we reach here, we have a TCP socket trying to be added into a epoll
  // instance. Convert the socket into VSOCK and resume to epoll_ctl.
  int fl = ConvertToVsock(fd);
  fcntl(fd, F_SETFL, fl);
  return libc_epoll_ctl(epfd, op, fd, event);
}

EXPORT int connect(int sockfd, const struct sockaddr* addr,
                   socklen_t addr_len) {
  // First of all, we only care about TCP sockets over IPv4 or IPv6. That is,
  // SOCK_STREAM type over AF_INET or AF_INET6. If any condition doesn't match
  // or even the getsockopt call fails, we fallback to libc_connect() so that
  // any error is handled by the pristine libc_connect().
  int sock_type = 0;
  socklen_t sock_type_len = sizeof(sock_type);
  int ret = libc_getsockopt(sockfd, SOL_SOCKET, SO_TYPE,
                            static_cast<void*>(&sock_type), &sock_type_len);
  int sock_domain = 0;
  socklen_t sock_domain_len = sizeof(sock_domain);
  // We need two libc_getsockopt() calls, one for SO_TYPE, one for SO_DOMAIN. If
  // either fails, fallback to libc_connect.
  if (ret != 0 || libc_getsockopt(sockfd, SOL_SOCKET, SO_DOMAIN,
                                  static_cast<void*>(&sock_domain),
                                  &sock_domain_len) != 0) {
    return libc_connect(sockfd, addr, addr_len);
  }
  // If:
  //    * the sockfd type is not SOCK_STREAM, or
  //    * sockfd domain is not IP/IPv6/VSOCK, or
  //    * target address is not IP/IPv6,
  // then fallback to libc connect().
  if (sock_type != SOCK_STREAM ||
      (sock_domain != AF_INET && sock_domain != AF_INET6 &&
       sock_domain != AF_VSOCK) ||
      (addr->sa_family != AF_INET && addr->sa_family != AF_INET6)) {
    return libc_connect(sockfd, addr, addr_len);
  }
  int fl = 0;
  if (sock_domain == AF_VSOCK) {
    fl = fcntl(sockfd, F_GETFL);
  } else {
    fl = ConvertToVsock(sockfd);
  }
  // Set blocking
  fcntl(sockfd, F_SETFL, (fl & ~O_NONBLOCK));
  sockaddr_vm vsock_addr = google::scp::proxy::GetProxyVsockAddr();
  if (constexpr auto vsock_addr_len = sizeof(vsock_addr);
      libc_connect(sockfd, reinterpret_cast<sockaddr*>(&vsock_addr),
                   vsock_addr_len) < 0) {
    fcntl(sockfd, F_SETFL, fl);
    return -1;
  }
  // Here this is a blocking call. This potentially hurts performance on many,
  // frequent, short non-blocking connections. However, without a blocking call
  // here we'd have to hijack select/poll/epoll all together as well, which is
  // far more complicated. They may be added later if needed.
  ret = socks5_client_connect(sockfd, addr);
  // Apply file modes again.
  fcntl(sockfd, F_SETFL, fl);
  return ret;
}

EXPORT int setsockopt(int sockfd, int level, int optname, const void* optval,
                      socklen_t optlen) throw() {
  // Application may still have the illusion that the socket is a TCP socket and
  // wants to set some TCP-level opts, e.g. TCP_NODELAY. Here we simply return 0
  // (success) in these scenarios, to avoid unnecessary failures.
  int sock_domain = 0;
  socklen_t sock_domain_len = sizeof(sock_domain);
  if ((level == IPPROTO_TCP || level == IPPROTO_IP || level == IPPROTO_IPV6) &&
      !libc_getsockopt(sockfd, SOL_SOCKET, SO_DOMAIN,
                       static_cast<void*>(&sock_domain), &sock_domain_len) &&
      sock_domain == AF_VSOCK) {
    return 0;
  }
  return libc_setsockopt(sockfd, level, optname, optval, optlen);
}

EXPORT int getsockopt(int sockfd, int level, int optname,
                      void* __restrict optval,
                      socklen_t* __restrict optlen) throw() {
  int sock_domain = 0;
  socklen_t sock_domain_len = sizeof(sock_domain);
  if ((level == IPPROTO_TCP || level == IPPROTO_IP || level == IPPROTO_IPV6) &&
      !libc_getsockopt(sockfd, SOL_SOCKET, SO_DOMAIN,
                       static_cast<void*>(&sock_domain), &sock_domain_len) &&
      sock_domain == AF_VSOCK) {
    return 0;
  }
  return libc_getsockopt(sockfd, level, optname, optval, optlen);
}

// A wrapper for resuming recv() call on EINTR.
// Return the total number of bytes received.
static ssize_t recv_all(int fd, void* buf, size_t len, int flags) {
  ssize_t received = 0;
  uint8_t* buffer = static_cast<uint8_t*>(buf);
  while (received < static_cast<ssize_t>(len)) {
    ssize_t r = recv(fd, buffer + received, len - received, flags);
    if (r < 0) {
      if (errno == EINTR) {
        continue;
      }
      // Otherwise, a real error happened
      return received;
    } else if (r == 0) {
      // Socket is shutdown, this is essentially an EOF.
      return received;
    } else {
      received += r;
    }
  }
  // Here we must have len == received. Return received anyway.
  return received;
}

// In java applications, at the end of plaintext connections (e.g. HTTP), java
// may call ioctl(FIONREAD) to get to know if there is any remaining data to
// consume. FIONREAD isn't supported on VSOCK, so we just fake it. To make sure
// most ioctl calls still follow the fastest path, here we still make the ioctl
// syscall first, and only on errors we kick-in and check if that's the case we
// want to handle.
EXPORT int ioctl(int fd, uint32_t request, void* argp) {  // NOLINT
#ifndef FIONREAD
#define FIONREAD 0x541B
#endif  // FIONREAD
  int ret = libc_ioctl(fd, request, argp);
  if (ret == -1 && errno == EOPNOTSUPP && request == FIONREAD) {
    int sock_domain = 0;
    socklen_t sock_domain_len = sizeof(sock_domain);
    int r = libc_getsockopt(fd, SOL_SOCKET, SO_DOMAIN,
                            static_cast<void*>(&sock_domain), &sock_domain_len);
    if (r == 0 && sock_domain == AF_VSOCK) {
      int* int_argp = static_cast<int*>(argp);
      *int_argp = 0;
      return 0;
    }
  }
  return ret;
}

int socks5_client_connect(int sockfd, const struct sockaddr* addr) {
  // To simplify the IO of the handshake process, we simply stuff everything we
  // want to send to server and send all at once.
  // Ref: https://datatracker.ietf.org/doc/html/rfc1928
  // out_buffer here will contain a client greeting declaring only supporting
  // one auth method "no auth",
  //     +----+----------+----------+
  //     |VER | NMETHODS | METHODS  |
  //     +----+----------+----------+
  //     | 1  |    1     | 1 to 255 |
  //     +----+----------+----------+
  // and a connect request, leaving the address type, address and port empty for
  // filling up later:
  //     +----+-----+-------+------+----------+----------+
  //     |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
  //     +----+-----+-------+------+----------+----------+
  //     | 1  |  1  | X'00' |  1   | Variable |    2     |
  //     +----+-----+-------+------+----------+----------+
  uint8_t buffer[64] = {0x05, 0x01, 0x00, 0x05, 0x01, 0x00};
  //                     |     |     |     |     |     |
  //              VER ---      |     |     |     |     |
  //         NMETHODS ---------      |     |     |     |
  // NO AUTH REQUIRED ---------------      |     |     |
  //      request VER ---------------------      |     |
  //      request CMD ---------------------------      |
  //      request RSV ---------------------------------

  size_t out_idx = 6;
  size_t copied = google::scp::proxy::FillAddrPort(&buffer[out_idx], addr);
  if (copied == 0) {
    return -1;
  }
  out_idx += copied;

  size_t out_size = out_idx;
  ssize_t ret = send(sockfd, buffer, out_size, 0);
  if (ret != static_cast<ssize_t>(out_size)) {
    return -1;
  }

  // Two messages has been sent. Receive replies now. Method selection reply:
  //     +----+--------+
  //     |VER | METHOD |
  //     +----+--------+
  //     | 1  |   1    |
  //     +----+--------+
  // Connection request reply:
  //     +----+-----+-------+------+----------+----------+
  //     |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
  //     +----+-----+-------+------+----------+----------+
  //     | 1  |  1  | X'00' |  1   | Variable |    2     |
  //     +----+-----+-------+------+----------+----------+

  static constexpr uint8_t kExpectedReply[] = {0x05, 0x00, 0x05, 0x00, 0x00};
  //                                        |     |     |     |     |
  //                                VER ----      |     |     |     |
  //                             METHOD ----------      |     |     |
  //                                VER ----------------      |     |
  //                                REP ----------------------      |
  //                                RSV ----------------------------

  // Reuse buffer here. Recv 2 more bytes to reveal the ATYP byte, and
  // potentially the length byte if the bound address is a domain name (see
  // DST.ADDR definition from rfc1928).
  constexpr auto kExpectedReplySize = sizeof(kExpectedReply) + 2;
  if (const ssize_t received = recv_all(sockfd, buffer, kExpectedReplySize, 0);
      received != kExpectedReplySize) {
    // Not enough data received. No way to proceed.
    return -1;
  }
  if (memcmp(buffer, kExpectedReply, sizeof(kExpectedReply)) != 0) {
    // Some error received. If there's a REP byte indicating errors, return the
    // REP byte inverted.
    if (buffer[3] != 0) {
      return -buffer[3];
    } else {
      return -1;
    }
  }
  uint8_t atyp = buffer[sizeof(kExpectedReply)];
  uint8_t extra_byte = buffer[sizeof(kExpectedReply) + 1];
  ssize_t to_receive = 0;
  if (atyp == 0x01) {
    // IPv4. 4-byte addr, 2-byte port, and we've already recv'd 1 byte extra.
    to_receive = 4 + 2 - 1;
  } else if (atyp == 0x03) {
    // Domain name. The length is defined by the first byte. Plus 2 bytes port.
    to_receive = extra_byte + 2;
  } else if (atyp == 0x04) {
    // IPv6. 16-byte addr, 2-byte port, and we've already recv'd 1 byte extra.
    to_receive = 16 + 2 - 1;
  }
  uint8_t* addr_buf = nullptr;
  if (to_receive + 1 > static_cast<ssize_t>(sizeof(buffer))) {
    // to_receive is larger than sizeof(buffer), so allocate a new one. This
    // should be rare as we don't expect server to send domain names as bound
    // address. However when it does, we should be able to handle it.
    addr_buf = reinterpret_cast<uint8_t*>(malloc(to_receive + 1));
    if (addr_buf == nullptr) {
      return -1;
    }
  } else {
    addr_buf = buffer;
  }
  addr_buf[0] = extra_byte;
  // Receive remaining bytes to drain the buffer.
  const ssize_t received =
      recv_all(sockfd, &addr_buf[1], to_receive, MSG_TRUNC);
  // TODO: We may remove MSG_TRUNC and make good use of the returned address.
  if (addr_buf != buffer) {
    free(addr_buf);
  }
  if (received != to_receive) {
    // Not enough data received. No way to proceed.
    return -1;
  }
  return 0;
}

EXPORT int bind(int sockfd, const struct sockaddr* addr,
                socklen_t addr_len) throw() {
  if (addr->sa_family != AF_INET && addr->sa_family != AF_INET6) {
    return libc_bind(sockfd, addr, addr_len);
  }
  // Check if it is STREAM socket, namely, TCP.
  int sock_type = 0;
  socklen_t sock_type_len = sizeof(sock_type);
  int ret = libc_getsockopt(sockfd, SOL_SOCKET, SO_TYPE,
                            static_cast<void*>(&sock_type), &sock_type_len);
  if (ret != 0 || sock_type != SOCK_STREAM) {
    return libc_bind(sockfd, addr, addr_len);
  }
  int sock_domain = 0;
  socklen_t sock_domain_len = sizeof(sock_domain);
  ret = libc_getsockopt(sockfd, SOL_SOCKET, SO_DOMAIN,
                        static_cast<void*>(&sock_domain), &sock_domain_len);
  // If the application calls epoll_ctl first and bind later, we may be dealing
  // with a VSOCK socket. So here it is either IP/IPv6 or VSOCK, otherwise
  // fallback.
  if (ret != 0 || (sock_domain != AF_VSOCK && sock_domain != AF_INET &&
                   sock_domain != AF_INET6)) {
    return libc_bind(sockfd, addr, addr_len);
  }
  uint16_t port = 0;
  if (addr->sa_family == AF_INET) {
    // If the socket family does not match the address to bind, fallback and let
    // libc_bind handle it. It's OK if it is VSOCK.
    if (sock_domain != AF_INET && sock_domain != AF_VSOCK) {
      return libc_bind(sockfd, addr, addr_len);
    }
    const sockaddr_in* v4addr = reinterpret_cast<const sockaddr_in*>(addr);
    port = ntohs(v4addr->sin_port);
  } else if (addr->sa_family == AF_INET6) {
    // If the socket family does not match the address to bind, fallback and let
    // libc_bind handle it. It's OK if it is VSOCK.
    if (sock_domain != AF_INET6 && sock_domain != AF_VSOCK) {
      return libc_bind(sockfd, addr, addr_len);
    }
    const sockaddr_in6* v6addr = reinterpret_cast<const sockaddr_in6*>(addr);
    port = ntohs(v6addr->sin6_port);
  } else {
    return libc_bind(sockfd, addr, addr_len);
  }
  // In the next few steps, replace sockfd with a UNIX domain socket.
  int uds_sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (uds_sock < 0) {
    return -1;
  }
  AutoCloseFd autoclose(uds_sock);
  // Preserve the file modes (esp. blocking/non-blocking) so that we can apply
  // the same modes later.
  int fl = fcntl(sockfd, F_GETFL);
  // "Atomically" close sockfd and duplicate the uds_sock into sockfd. So that
  // the application can use the same fd value. This essentially changes the
  // sockfd family from AF_INET(6) to AF_UNIX
  if (dup2(uds_sock, sockfd) < 0) {
    return -1;
  }
  struct sockaddr_un uds_addr = {
      .sun_family = AF_UNIX,
  };
  constexpr auto uds_addr_len = sizeof(uds_addr);
  memcpy(uds_addr.sun_path, google::scp::proxy::kSocketVendorUdsPath.data(),
         google::scp::proxy::kSocketVendorUdsPath.size());
  if (libc_connect(sockfd, reinterpret_cast<sockaddr*>(&uds_addr),
                   uds_addr_len) < 0) {
    return -1;
  }
  // In the next few steps, perform socket vendor requests.
  socket_vendor::BindRequest bind_req;
  bind_req.port = port;
  constexpr auto bind_req_len = sizeof(bind_req);
  if (const ssize_t num_bytes = send(sockfd, &bind_req, bind_req_len, 0);
      num_bytes != static_cast<ssize_t>(bind_req_len)) {
    return -1;
  }
  socket_vendor::BindResponse bind_resp;
  const auto bind_resp_len = sizeof(bind_resp);
  if (const ssize_t num_bytes = recv(sockfd, &bind_resp, bind_resp_len, 0);
      num_bytes != static_cast<ssize_t>(bind_resp_len)) {
    return -1;
  }
  if (bind_resp.type != socket_vendor::MessageType::kBindResponse) {
    return -1;
  }
  // Apply file modes again.
  fcntl(sockfd, F_SETFL, fl);
  return 0;
}

EXPORT int listen(int sockfd, int backlog) throw() {
  int sock_domain = 0;
  socklen_t sock_domain_len = sizeof(sock_domain);
  if (libc_getsockopt(sockfd, SOL_SOCKET, SO_DOMAIN,
                      static_cast<void*>(&sock_domain), &sock_domain_len)) {
    return libc_listen(sockfd, backlog);
  }
  if (sock_domain != AF_UNIX) {
    return libc_listen(sockfd, backlog);
  }
  sockaddr_un uds_addr;
  socklen_t uds_addr_len = sizeof(uds_addr);
  if (int ret = getpeername(sockfd, reinterpret_cast<sockaddr*>(&uds_addr),
                            &uds_addr_len);
      ret < 0) {
    return libc_listen(sockfd, backlog);
  }
  // TODO: we may add more strict check here

  socket_vendor::ListenRequest listen_req;
  listen_req.backlog = backlog;
  constexpr auto listen_req_len = sizeof(listen_req);
  if (const ssize_t num_bytes = send(sockfd, &listen_req, listen_req_len, 0);
      num_bytes != listen_req_len) {
    return -1;
  }
  int fl = fcntl(sockfd, F_GETFL);
  fcntl(sockfd, F_SETFL, fl & ~O_NONBLOCK);
  socket_vendor::ListenResponse listen_resp;
  constexpr auto listen_resp_len = sizeof(listen_resp);
  if (const ssize_t num_bytes =
          recv(sockfd, &listen_resp, listen_resp_len, MSG_WAITALL);
      num_bytes != listen_resp_len) {
    return -1;
  } else if (listen_resp.type != socket_vendor::MessageType::kListenResponse) {
    return -1;
  }
  fcntl(sockfd, F_SETFL, fl);
  return 0;
}

EXPORT int accept4(int sockfd, struct sockaddr* addr, socklen_t* addr_len,
                   int flags) {
  int sock_domain = 0;
  socklen_t sock_domain_len = sizeof(sock_domain);
  if (libc_getsockopt(sockfd, SOL_SOCKET, SO_DOMAIN,
                      static_cast<void*>(&sock_domain), &sock_domain_len)) {
    return libc_accept4(sockfd, addr, addr_len, flags);
  }
  if (sock_domain != AF_UNIX) {
    return libc_accept4(sockfd, addr, addr_len, flags);
  }
  // There might be use cases that the application uses unix domain socket for
  // communication. Here we check if sockfd is in a connected state by calling
  // getpeername(), if so, then it is a socket under our manipulation.
  // TODO: we can use a global bitmap to identify the socket, if the following
  // logic turns out to be hurting performance.
  sockaddr_un uds_addr;
  socklen_t uds_addr_len = sizeof(uds_addr);
  if (const int ret = getpeername(
          sockfd, reinterpret_cast<sockaddr*>(&uds_addr), &uds_addr_len);
      ret < 0) {
    return libc_accept4(sockfd, addr, addr_len, flags);
  }

  // Now prepare for accepting a file descriptor
  socket_vendor::NewConnectionResponse resp;
  struct iovec iov = {
      .iov_base = &resp,
      .iov_len = sizeof(resp),
  };

  union {
    struct cmsghdr align;
    char buf[CMSG_SPACE(sizeof(int))];
  } cmsgu;
  struct msghdr msg = {
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = cmsgu.buf,
      .msg_controllen = sizeof(cmsgu.buf),
  };

  if (ssize_t num_bytes = recvmsg(sockfd, &msg, 0); num_bytes < 0) {
    // Note that this might be a benign failure when sockfd is made
    // non-blocking. recv() might return with EAGAIN/EWOULDBLOCK, which is also
    // expected errno for accept() calls.
    return -1;
  }
  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  if (cmsg == nullptr) {
    errno = EBADF;
    return -1;
  }
  if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
    errno = EBADF;
    return -1;
  }
  int fd = -1;
  memcpy(&fd, CMSG_DATA(cmsg), sizeof(fd));
  if (fd < 0) {
    errno = EBADF;
    return -1;
  }
  int fl = fcntl(fd, F_GETFL);
  if (flags | SOCK_CLOEXEC) {
    fl |= FD_CLOEXEC;
  }
  if (flags | SOCK_NONBLOCK) {
    fl |= O_NONBLOCK;
  }
  fcntl(fd, F_SETFL, fl);
  if (addr == nullptr) {
    return fd;
  }
  // From this point, we don't know if the original listener socket was created
  // as IPv6 or IPv4 socket. So we identify it by looking at socklen.
  constexpr auto kSockAddrIpv6Len =
      static_cast<socklen_t>(sizeof(sockaddr_in6));
  if (*addr_len >= kSockAddrIpv6Len) {
    struct sockaddr_in6 v6addr = {
        .sin6_family = AF_INET6,
        .sin6_port = resp.port,
    };
    memcpy(&v6addr.sin6_addr, resp.addr, sizeof(resp.addr));
    memcpy(addr, &v6addr, kSockAddrIpv6Len);
  } else {
    struct sockaddr_in v4addr = {
        .sin_family = AF_INET,
        .sin_port = resp.port,
    };
    constexpr auto kSockAddrIpv4Len =
        static_cast<socklen_t>(sizeof(sockaddr_in));
    // Determine if the address is convertible to IPv4. If so, convert it.
    uint32_t uint_addr[4];
    memcpy(uint_addr, resp.addr, sizeof(uint_addr));
    if (uint_addr[0] == 0 && uint_addr[1] == 0 && uint_addr[2] == 0 &&
        uint_addr[3] != 1) {
      // IPv4 compatible address
      memcpy(&v4addr.sin_addr, &uint_addr[3], sizeof(v4addr.sin_addr));
    } else if (uint_addr[0] == 0 && uint_addr[1] == 0 &&
               ntohl(uint_addr[2]) == 0xFFFF) {
      // IPv4-mapped address
      memcpy(&v4addr.sin_addr, &uint_addr[3], sizeof(v4addr.sin_addr));
    }
    *addr_len = std::min(*addr_len, kSockAddrIpv4Len);
    memcpy(addr, &v4addr, *addr_len);
  }
  return fd;
}

EXPORT int accept(int sockfd, struct sockaddr* addr, socklen_t* addr_len) {
  return accept4(sockfd, addr, addr_len, 0);
}

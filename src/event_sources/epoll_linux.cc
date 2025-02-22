// Copyright (C) 2018 Toitware ApS.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; version
// 2.1 only.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// The license can be found in the file `LICENSE` in the top level
// directory of this repository.

#include "../top.h"

#ifdef TOIT_LINUX

#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#include <cerrno>

#include "../objects_inline.h"

#include "epoll_linux.h"

namespace toit {

enum {
  kAdd,
  kRemove,
};

bool write_full(int fd, uint8_t* data, int length) {
  int offset = 0;
  while (offset < length) {
    int wrote = write(fd, data + offset, length - offset);
    if (wrote < 0 && errno != EINTR) {
      return false;
    }
    offset += wrote;
  }
  return true;
}

bool read_full(int fd, uint8_t* data, int length) {
  int offset = 0;
  while (offset < length) {
    int wrote = read(fd, data + offset, length - offset);
    if (wrote < 0 && errno != EINTR) {
      return false;
    }
    offset += wrote;
  }
  return true;
}

EpollEventSource* EpollEventSource::_instance = null;

EpollEventSource::EpollEventSource() : EventSource("Epoll")
    , Thread("Epoll") {
  ASSERT(_instance == null);
  _instance = this;

  _epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  if (_epoll_fd < 0) {
    FATAL("failed allocating epoll file descriptor: %d", errno)
  }

  int fds[2];
  if (pipe2(fds, O_CLOEXEC | O_NONBLOCK) != 0) {
    FATAL("failed allocating pipe file descriptors: %d", errno)
  }

  _control_read = fds[0];
  _control_write = fds[1];

  epoll_event event = {0, {0}};
  event.events = EPOLLIN | EPOLLOUT;
  event.data.fd = _control_read;
  if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _control_read, &event) == -1) {
    FATAL("failed to register close fd: %d\n", errno);
  }

  spawn();
}

EpollEventSource::~EpollEventSource() {
  close(_control_write);
  join();
  close(_epoll_fd);

  _instance = null;
}


void EpollEventSource::on_register_resource(Locker& locker, Resource* r) {
  auto resource = static_cast<IntResource*>(r);
  uint64_t cmd = resource->id();
  cmd <<= 32;
  cmd |= kAdd;
  if (!write_full(_control_write, reinterpret_cast<uint8_t*>(&cmd), sizeof(cmd))) {
    FATAL("failed to send 0x%llx to epoll: %d", cmd, errno);
  }
}

void EpollEventSource::on_unregister_resource(Locker& locker, Resource* r) {
  auto resource = static_cast<IntResource*>(r);
  uint64_t cmd = resource->id();
  cmd <<= 32;
  cmd |= kRemove;
  if (!write_full(_control_write, reinterpret_cast<uint8_t*>(&cmd), sizeof(cmd))) {
    FATAL("failed to send 0x%llx to epoll: %d", cmd, errno);
  }
}

void EpollEventSource::entry() {
  while (true) {
    epoll_event event;
    int ready = epoll_wait(_epoll_fd, &event, 1, -1);
    switch (ready) {
      case 1: {
        if (event.data.fd == _control_read) {
          if (event.events & EPOLLHUP) {
            close(_control_read);
            return;
          }

          uint64_t cmd = 0;
          if (!read_full(_control_read, reinterpret_cast<uint8_t*>(&cmd), sizeof(cmd))) {
            FATAL("failed to receive 0x%llx in epoll: %d", cmd, errno);
          }

          int id = cmd >> 32;
          switch (cmd & ((1LL << 32) - 1)) {
            case kAdd: {
                epoll_event event = {0, {0}};
                event.events = EPOLLIN | EPOLLOUT | EPOLLET;
                event.data.fd = id;
                if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, id, &event) == -1) {
                  FATAL("failed to add 0x%lx to epoll: %d", id, errno);
                }
              }
              break;

            case kRemove: {
                if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, id, null) == -1) {
                  FATAL("failed to add 0x%lx to epoll: %d", id, errno);
                }
                // Don't close STD pipes.
                if (id > 2) close(id);
              }
              break;
          }

          continue;
        }

        Locker locker(mutex());
        Resource* r = find_resource_by_id(locker, event.data.fd);
        if (r != null) dispatch(locker, r, event.events);
        break;
      }

      case 0:
        // No events (timeout), loop.
        break;

      case  -1: {
        if (errno == EINTR) {
          // No events (timeout), loop.
          break;
        }
        // FALL THROUGH.
      }

      default:
        FATAL("error waiting for epoll events");
        break;
    }
  }
}



} // namespace toit

#endif // TOIT_LINUX

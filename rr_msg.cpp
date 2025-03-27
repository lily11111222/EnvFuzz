/*
 *  ____  ____  _____              
 * |  _ \|  _ \|  ___|   _ ________
 * | |_) | |_) | |_ | | | |_  /_  /
 * |  _ <|  _ <|  _|| |_| |/ / / / 
 * |_| \_\_| \_\_|   \__,_/___/___|
 *
 * Copyright (C) National University of Singapore
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

 
 struct MSG
 {
     MSG *next;              // Next message
     MSG *prev;              // Prev message
     uint16_t port;          // Message port
     int8_t error;           // Message error code (if applicable)
     bool outbound;          // Message direction
     uint32_t id;            // Message ID
     union
     {
         uint32_t len;       // Payload length
         int32_t slen;       // Payload length (signed)
     };
     uint8_t payload[];      // Payload (pointer to data[])
 };
 
 #include "fuzz_mutate.cpp"
 #include "fuzz_patch.cpp"
 #include "fuzz_feedback.cpp"
 #include "fuzz_main.cpp"
 
 struct QUEUE
 {
     void *root;             // Tree root
 };
 
 static int msg_compare(const void *a, const void *b)
 {
     const MSG *M = (MSG *)a, *N = (MSG *)b;
     return M->port - N->port;
 }
 
 static void *queue_push(QUEUE *Q, MSG *M)
 {
     void *node = tfind(M, &Q->root, msg_compare);
     if (node == NULL)
     {
         M->next = M->prev = M;
         return tsearch(M, &Q->root, msg_compare);
     }
     MSG *N = *(MSG **)node;
     MSG *P = N->prev;
     M->next = N;
     M->prev = P;
     P->next = N->prev = M;
     return node;
 }
 
 static void queue_push_front(QUEUE *Q, MSG *M)
 {
     void *node = queue_push(Q, M);
     assert(node != NULL);
     *(MSG **)node = M;
 }
 
 static void queue_push_back(QUEUE *Q, MSG *M)
 {
     (void)queue_push(Q, M);
 }
 
 static MSG *queue_pop(QUEUE *Q, int port)
 {
     MSG K;
     K.port = port;
     void *node = tfind(&K, &Q->root, msg_compare);
     if (node == NULL)
         return NULL;
     MSG *N = *(MSG **)node;
     if (N->next == N)
         (void)tdelete(N, &Q->root, msg_compare);
     else
     {
         N->prev->next = N->next;
         N->next->prev = N->prev;
         *(MSG **)node = N->next;
     }
     return N;
 }
 
 static MSG *queue_peek(QUEUE *Q, int port)
 {
     MSG K;
     K.port = port;
     void *node = tfind(&K, &Q->root, msg_compare);
     return (node == NULL? NULL: *(MSG **)node);
 }
 
 static bool queue_reorder(QUEUE *Q, int port)
 {
     MSG K;
     K.port = port;
     void *node = tfind(&K, &Q->root, msg_compare);
     if (node == NULL)
         return false;
     MSG *H = *(MSG **)node;
     MSG *N = H->next;
     while (N != H && N->outbound)
         N = N->next;
     if (N == H)
         return false;
     N->prev->next = N->next;
     N->next->prev = N->prev;
     N->next = H;
     N->prev = H->prev;
     H->prev->next = N;
     H->prev = N;
     *(MSG **)node = N;
     return true;
 }
 
 static void queue_purge(QUEUE *Q, int port)
 {
     MSG K;
     K.port = port;
     (void)tdelete(&K, &Q->root, msg_compare);
 }

 bool is_user_ubuntu(ENTRY *E) {
    // fprintf(stderr, "msg->payload: %s", (const char *)msg->payload);
    // 目标字符串及其长度
    const char *target = "port://";
    const size_t target_len = strlen(target);  // target_len = 11

    // 校验 payload 长度是否足够
    if (strlen(E->name) < target_len) {
        return false;
    }

    // 比较前 target_len 字节
    return memcmp(E->name, target, target_len) == 0;
}
 
 /*
  * Get input from the queue.
  */
 static ssize_t queue_read(iovec *iov, size_t iovcnt, int fd)
 {
     ENTRY *E = fd_get(fd);
     QUEUE *Q = option_Q;
     MSG *M = queue_pop(Q, E->port);
     if (M == NULL)
     {
         // No more data:
         return 0;
     }
     if (M->outbound)
     {
         // Wrong direction:
         queue_push_front(Q, M);
         return -EAGAIN;
     }
 
     MSG *N = M;
     if (option_patch)
         M = patch_next(M, option_P);
     if (option_fuzz && E->mutate) {
         warning("queue_read: option_fuzz && E->mutate");
          M = fuzzer_mutate(E, M, is_user_ubuntu(E));
     }
     if (M != N && option_log >= 2)
     {
         struct iovec iov1 = {M->payload, M->len};
         struct iovec iov2 = {N->payload, N->len};
         PRINTER Q;
         print_diff(Q, &iov1, 1, &iov2, 1);
         fprintf(stderr, "%s\n", Q.str());
     }
 
     if (M != N && option_fuzz)
         FUZZ->patch->push_back(M);
 
     struct iovec iov2 = {M->payload, M->len};
     ssize_t r = (ssize_t)iov_copy(iov, iovcnt, &iov2, 1, SIZE_MAX);
     if (E->event.enabled)
         eventfd_check_read(E, iov, iovcnt);
     return r;
 }
 static ssize_t queue_read(uint8_t *buf, size_t size, int fd)
 {
     struct iovec iov;
     iov.iov_base = (void *)buf;
     iov.iov_len  = size;
     return queue_read(&iov, 1, fd);
 }
 
 /*
  * Emulated read input from queue.
  */
 static ssize_t queue_emulate_read(iovec *iov, size_t iovcnt, int fd)
 {
     ENTRY *E = fd_get(fd);
     if (E->event.enabled)
         return eventfd_emulate_read(E, iov, iovcnt);
 
     QUEUE *Q = option_Q;
     warning("E->port: %d", E->port);
     MSG *M   = queue_peek(Q, E->port);
     if (M == NULL)
     {
         switch (E->eof++)
         {
             case 0: case 1:
                 return (E->filetype == S_IFSOCK? -ECONNRESET: 0);
             case 2: 
                 if (E->filetype != S_IFSOCK)
                     return -EIO;
                 // Fallthrough:
             default:
                 error("program-under-test ignores EOF for (%s)", E->name);
         }
     }
     if (M->outbound)
         return -EAGAIN;
     (void)queue_pop(Q, E->port);
 
     MSG *N = M;
     if (option_patch)
         M = patch_next(M, option_P);
     if (option_fuzz && E->mutate){
         warning("option_fuzz && E->mutate");
         M = fuzzer_mutate(E, M, is_user_ubuntu(E));
     } 
     if (M != N && option_fuzz)
         FUZZ->patch->push_back(M);
 
     struct iovec iov2 = {M->payload, M->len};
     ssize_t r = (ssize_t)iov_copy(iov, iovcnt, &iov2, 1, SIZE_MAX);
     return r;
 }
 
 /*
  * Emulated write output.
  */
 static ssize_t queue_emulate_write(iovec *iov, size_t iovcnt, int fd)
 {
     ENTRY *E = fd_get(fd);
     if (E->event.enabled)
         return eventfd_emulate_write(E, iov, iovcnt);
 
     if (E->eof > 0)
         return (E->filetype != S_IFSOCK? -EIO: -ECONNRESET);
     QUEUE *Q = option_Q;
     MSG *M   = queue_peek(Q, E->port);
     if (M != NULL && M->outbound)
     {
         M = queue_pop(Q, E->port);
         xfree(M);
     }
 
     if (option_log >= 1 && option_log <= 2 &&
         (fd == STDOUT_FILENO || fd == STDERR_FILENO))
     {
         PRINTER P;
         print_output(P, iov, iovcnt);
         fprintf(stderr, "%s%s%s", CYAN, P.str(), OFF);
     }
 
     ssize_t r = (ssize_t)iov_len(iov, iovcnt);
     return r;
 }
 
 /*
  * Emulated I/O operation.
  */
 static ssize_t queue_emulate_get(iovec *iov, size_t iovcnt, int fd,
     bool outbound)
 {
     if (outbound)
         return queue_emulate_write(iov, iovcnt, fd);
     else
         return queue_emulate_read(iov, iovcnt, fd);
 }
 static ssize_t queue_emulate_get(uint8_t *buf, size_t size, int fd,
     bool outbound)
 {
     struct iovec iov;
     iov.iov_base = (void *)buf;
     iov.iov_len  = size;
     return queue_emulate_get(&iov, 1, fd, outbound);
 }
 
 /*
  * Emulated poll() syscall.
  */
 static int queue_emulate_poll(struct pollfd *fds, nfds_t nfds, int timeout)
 {
     if (nfds < 0) return -EINVAL;
     if (nfds == 0) return 0;
     bool retry = true;
 
     // Step (1): Satisfy the request "normally":
 retry: {}
     QUEUE *Q = option_Q;
     int seen = 0, hup = 0, count = 0;
     for (nfds_t i = 0; i < nfds; i++)
     {
         fds[i].revents = 0x0;
         if (fds[i].fd < 0)
             continue;
         ENTRY *E = fd_entry(fds[i].fd);
         if (E == NULL)
         {
             count++;
             fds[i].revents = POLLNVAL;
             continue;
         }
 //        fprintf(stderr, "%sPOLL%s: %s [%d]\n", BLUE, OFF, E->name,
 //            E->fd);
         if (E->event.enabled)
         {
             short revents = (fds[i].events & eventfd_emulate_poll(E));
             if (revents != 0)
             {
                 fds[i].revents = revents;
                 count++;
             }
             seen++;
             continue;
         }
         MSG *M = queue_peek(Q, E->port);
         if (M == NULL)
         {
             E->eof++;
             if (E->eof > 4)
                 error("program-under-test ignores EOF for (%s)", E->name);
             hup++;
             fds[i].revents = (E->filetype == S_IFSOCK? POLLERR: POLLHUP);
             continue;
         }
         if ((fds[i].events & (POLLIN | POLLOUT)) == 0x0)
             continue;
         seen++;
         short revents = (fds[i].events & (M->outbound? POLLOUT: POLLIN));
         if (revents != 0x0)
         {
             fds[i].revents = revents;
             count++;
         }
     }
     if (count > 0 || timeout == 0)
         return count;
 
     // Step (2): Try switching threads:
     if (retry)
     {
 //        fprintf(stderr, "%sBLOCK%s\n", BLUE, OFF);
         retry = false;
         FIBER_NEXT();
         goto retry;
     }
     if (seen == 0 || hup > 0)
         return count;
 
     // Step (3): Allow POLLOUT (if any):
     for (nfds_t i = 0; i < nfds; i++)
     {
         if (fds[i].fd < 0)
             continue;
         if ((fds[i].events & POLLOUT) == 0)
             continue;
         fds[i].revents = POLLOUT;
         count++;
     }
     if (count > 0)
         return count;
 
     // Step (4): Allow POLLIN by reordering (if any):
     ssize_t min_i = -1, min_d = /*MAX=*/16;
     for (nfds_t i = 0; i < nfds; i++)
     {
         if (fds[i].fd < 0)
             continue;
         if ((fds[i].events & POLLIN) == 0)
             continue;
         ENTRY *E = fd_entry(fds[i].fd);
         if (E->event.enabled)
             continue;
         MSG *M = queue_peek(Q, E->port);
         ssize_t d = 0;
         for (MSG *N = M->next; d < min_d && N != M; N = N->next)
         {
             if (!N->outbound)
             {
                 min_i = i;
                 min_d = d;
             }
         }
     }
     if (min_i >= 0)
     {
         ENTRY *E = fd_entry(fds[min_i].fd);
         bool ok = queue_reorder(Q, E->port);
         assert(ok);
         fds[min_i].revents = POLLIN;
         return 1;
     }
 
     // Step (5): Fail the operation
     for (nfds_t i = 0; i < nfds; i++)
     {
         if (fds[i].fd < 0)
             continue;
         ENTRY *E = fd_entry(fds[i].fd);
         queue_purge(Q, E->port);
         fds[i].revents = POLLERR;
         E->eof++;
     }
     return 0;
 }
 
 /*
  * Emulated poll() syscall.
  */
 static int queue_emulate_select(int nfds, fd_set *rfds, fd_set *wfds,
     fd_set *efds, int timeout)
 {
     // We translate this into a poll() syscall:
     struct pollfd fds[nfds];
     int j = 0;
     for (int i = 0; i < nfds; i++)
     {
         if (fd_entry(i) == NULL)
             continue;
         fds[j].fd = i;
         fds[j].events = 0x0;
         if (rfds != NULL && FD_ISSET(i, rfds))
             fds[j].events |= POLLIN;
         if (wfds != NULL && FD_ISSET(i, wfds))
             fds[j].events |= POLLOUT;
         if (fds[j].events == 0)
             continue;
         j++;
     }
     nfds = j;
     int r = queue_emulate_poll(fds, nfds, timeout);
     if (r < 0)
         return r;
     r = 0;
     for (int i = 0, k = 0; k < nfds; i++)
     {
         if (i != fds[k].fd)
         {
             if (rfds != NULL) FD_CLR(i, rfds);
             if (wfds != NULL) FD_CLR(i, wfds);
             if (efds != NULL) FD_CLR(i, efds);
             continue;
         }
         short poll_read  = (POLLIN  | POLLHUP);
         short poll_write = (POLLOUT | POLLHUP);
         short poll_err   = POLLERR;
         short events     = fds[k].events;
         r += (rfds != NULL && (events & poll_read)?  1: 0);
         r += (wfds != NULL && (events & poll_write)? 1: 0);
         r += (efds != NULL && (events & poll_err)?   1: 0);
         if (rfds != NULL && (events & poll_read)  == 0) FD_CLR(i, rfds);
         if (wfds != NULL && (events & poll_write) == 0) FD_CLR(i, wfds);
         if (efds != NULL && (events & poll_err)   == 0) FD_CLR(i, efds);
         k++;
     }
     return r;
 }
 
 /*
  * Emulated epoll_wait() syscall.
  */
 static int queue_emulate_epoll_wait(int efd, struct epoll_event *events,
     int maxevents, int timeout)
 {
     // We translate this into a poll() syscall:
     const ENTRY *E = fd_entry(efd);
     if (E == nullptr || events == NULL || maxevents <= 0)
         return -EINVAL;
     int nfds = 0, i = 0, j = 0;
     for (const EPOLL *info = E->epoll; info != NULL; info = info->next)
         nfds++;
     if (nfds == 0)
         return 0;
     struct pollfd fds[nfds];
     for (const EPOLL *info = E->epoll; info != NULL; info = info->next)
     {
         fds[i].fd     = info->fd;
         fds[i].events =
             (info->event.events & EPOLLIN?  POLLIN:  0x0) |
             (info->event.events & EPOLLOUT? POLLOUT: 0x0);
         fds[i].revents = 0x0;
         i++;
     }
     int r = queue_emulate_poll(fds, nfds, timeout);
     if (r < 0)
         return r;
     i = 0;
     for (const EPOLL *info = E->epoll; info != NULL; info = info->next)
     {
         if (fds[i].revents != 0x0)
         {
             events[j].events = 
                 (fds[i].revents & POLLIN?  EPOLLIN:  0x0) |
                 (fds[i].revents & POLLOUT? EPOLLOUT: 0x0) |
                 (fds[i].revents & POLLHUP? EPOLLHUP: 0x0) |
                 (fds[i].revents & POLLERR? EPOLLERR: 0x0);
             memcpy(&events[j].data, &info->event.data, sizeof(events[j].data));
             j++;
             if (j >= maxevents)
                 break;
         }
         i++;
     }
     return j;
 }
 
 
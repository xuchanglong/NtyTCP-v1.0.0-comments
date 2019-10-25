/*
 * MIT License
 *
 * Copyright (c) [2018] [WangBoJing]

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 *
****       *****                                  ************
  ***        *                                    **   **    *
  ***        *         *                          *    **    **
  * **       *         *                         *     **     *
  * **       *         *                         *     **     *
  *  **      *        **                               **
  *  **      *       ***                               **
  *   **     *    ***********    *****    *****        **             *****         *  ****
  *   **     *        **           **      **          **           ***    *     **** *   **
  *    **    *        **           **      *           **           **     **      ***     **
  *    **    *        **            *      *           **          **      **      **       *
  *     **   *        **            **     *           **         **       **      **       **
  *     **   *        **             *    *            **         **               **       **
  *      **  *        **             **   *            **         **               **       **
  *      **  *        **             **   *            **         **               **       **
  *       ** *        **              *  *             **         **               **       **
  *       ** *        **              ** *             **         **               **       **
  *        ***        **               * *             **         **         *     **       **
  *        ***        **     *         **              **          **        *     **      **
  *         **        **     *         **              **          **       *      ***     **
  *         **         **   *          *               **           **     *       ****   **
*****        *          ****           *             ******           *****        **  ****
                                       *                                           **
                                      *                                            **
                                  *****                                            **
                                  ****                                           ******

 *
 */

#ifndef __NTY_SOCKET_H__
#define __NTY_SOCKET_H__

#include "nty_buffer.h"
#include "nty_tcp.h"
#include "nty_config.h"

#include <pthread.h>

typedef struct _nty_socket_map
{
    int id;
    int socktype;
    uint32_t opts;

    struct sockaddr_in s_addr;

    union {
        struct _nty_tcp_stream *stream;
        struct _nty_tcp_listener *listener;
#if NTY_ENABLE_EPOLL_RB
        void *ep;
#else
        struct _nty_epoll *ep;
#endif
        //struct pipe *pp;
    };

    uint32_t epoll;
    uint32_t events;
    uint64_t ep_data;

    TAILQ_ENTRY(_nty_socket_map)
    free_smap_link;
} nty_socket_map; //__attribute__((packed))

enum nty_socket_opts
{
    NTY_TCP_NONBLOCK = 0x01,
    NTY_TCP_ADDR_BIND = 0x02,
};

nty_socket_map *nty_allocate_socket(int socktype, int need_lock);
void nty_free_socket(int sockid, int need_lock);
nty_socket_map *nty_get_socket(int sockid);

/*
 * rebuild socket module for support 10M
 */
#if NTY_ENABLE_SOCKET_C10M

struct _nty_socket
{
    /**
     * socket 描述符的值。
    */
    int id;

    /**
     * socket 类型。
    */
    int socktype;

    uint32_t opts;
    struct sockaddr_in s_addr;

    /**
     * 表示当前 socket 的应用领域。
    */
    union {
        struct _nty_tcp_stream *stream;
        struct _nty_tcp_listener *listener;
        /**
         * epoll 领域。
        */
        void *ep;
    };

    /**
     * 该 socket 所在的 socket 列表的地址。
    */
    struct _nty_socket_table *socktable;
};

struct _nty_socket_table
{
    /**
     * 保存所有的 socket 描述符所需要的字节数量，
     * 即：open_fds 数组的大小。
    */
    size_t max_fds;

    /**
     * 当前 socket 描述符所在的数组的位置。
    */
    int cur_idx;

    /**
     * 存储 _nty_socket 的数组。
    */
    struct _nty_socket **sockfds;

    /**
     * 用于记录所有的 socket 使用与否的数组。
     * 用 bit 表示 socket 使用与否。
    */
    unsigned char *open_fds;

    /**
     * 保证对 _nty_socket_table 操作同步的自旋锁。
    */
    pthread_spinlock_t lock;
};

/**
 * @function    根据 socket 类型，创建 _nty_socket 。
 *              分配 socket 描述符的值。
 *              其他变量进行初始化。
 * @paras   socktype    指定的 socket 的类型。
 * @ret 非nullptr   创建成功。
 *      nullptr 创建失败。
*/
struct _nty_socket *nty_socket_allocate(int socktype);

void nty_socket_free(int sockid);

struct _nty_socket *nty_socket_get(int sockid);

struct _nty_socket_table *nty_socket_init_fdtable(void);

int nty_socket_close_listening(int sockid);

int nty_socket_close_stream(int sockid);

#endif

#endif

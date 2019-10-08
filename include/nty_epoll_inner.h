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

#ifndef __NTY_EPOLL_INNER_H__
#define __NTY_EPOLL_INNER_H__

#include "nty_socket.h"
#include "nty_epoll.h"
#include "nty_buffer.h"
#include "nty_header.h"

typedef struct _nty_epoll_stat
{
	uint64_t calls;
	uint64_t waits;
	uint64_t wakes;

	uint64_t issued;
	uint64_t registered;
	uint64_t invalidated;
	uint64_t handled;
} nty_epoll_stat;

typedef struct _nty_epoll_event_int
{
	nty_epoll_event ev;
	int sockid;
} nty_epoll_event_int;

typedef enum
{
	USR_EVENT_QUEUE = 0,
	USR_SHADOW_EVENT_QUEUE = 1,
	NTY_EVENT_QUEUE = 2
} nty_event_queue_type;

typedef struct _nty_event_queue
{
	nty_epoll_event_int *events;
	int start;
	int end;
	int size;
	int num_events;
} nty_event_queue;

typedef struct _nty_epoll
{
	nty_event_queue *usr_queue;
	nty_event_queue *usr_shadow_queue;
	nty_event_queue *queue;

	uint8_t waiting;
	nty_epoll_stat stat;

	pthread_cond_t epoll_cond;
	pthread_mutex_t epoll_lock;
} nty_epoll;

int nty_epoll_add_event(nty_epoll *ep, int queue_type, struct _nty_socket_map *socket, uint32_t event);
int nty_close_epoll_socket(int epid);
int nty_epoll_flush_events(uint32_t cur_ts);

#if NTY_ENABLE_EPOLL_RB

struct epitem
{
	/**
     * struct {													
	 * struct epitem *rbe_left;
	 * struct epitem *rbe_right;	
	 * struct epitem *rbe_parent;
	 * int rbe_color;	
     * } rbn
    */
	RB_ENTRY(epitem) rbn;

	/**
     * struct                                          
	 * {                                        
	 *  struct type *le_next;  
	 *  struct type **le_prev;
	 * } rdlink
     */
	LIST_ENTRY(epitem) rdlink;

	/**
	 * 标记该节点是否在双向链表中。
	*/
	int rdy;

	/**
	 * 该节点对应的 socket 描述符。
	*/
	int sockfd;

	/**
	 * 保存需要 epoll 机制需要监控的事件以及其他数据。
	*/
	struct epoll_event event;
};

static int sockfd_cmp(struct epitem *ep1, struct epitem *ep2)
{
	if (ep1->sockfd < ep2->sockfd)
		return -1;
	else if (ep1->sockfd == ep2->sockfd)
		return 0;
	return 1;
}

RB_HEAD(_epoll_rb_socket, epitem);
RB_GENERATE_STATIC(_epoll_rb_socket, epitem, rbn, sockfd_cmp);

typedef struct _epoll_rb_socket ep_rb_tree;

struct eventpoll
{
	/**
	 * 保存红黑树根节点。
	 * struct _epoll_rb_socket
	 * {								
	 * 	struct epitem *rbh_root; 
	 * } rbr;
	*/
	ep_rb_tree rbr;
	int rbcnt;

	/**
	 * 双向链表的根。
	 * struct
	 * {
	 * 		struct epitem *lh_first
	 * }
	 * rdlist;
	*/
	LIST_HEAD(, epitem)
	rdlist;

	/**
	 * 双向链表中节点的数目，即：指定的事件发生的数目。
	*/
	int rdnum;

	int waiting;

	pthread_mutex_t mtx;	 //rbtree update
	pthread_spinlock_t lock; //rdlist update

	pthread_cond_t cond;   //block for event
	pthread_mutex_t cdmtx; //mutex for cond
};

/**
 * @function	该函数是回调函数，被网卡驱动调用。
 * 				当 client 向 server 发出三次握手、可写、可读和断开连接时，网卡会调用该函数，向双向链表中插入 epitem 。	
*/
int epoll_event_callback(struct eventpoll *ep, int sockid, uint32_t event);

#endif

#endif

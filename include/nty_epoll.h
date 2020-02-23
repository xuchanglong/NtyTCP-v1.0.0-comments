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

#ifndef __NTY_EPOLL_H__
#define __NTY_EPOLL_H__

#include <stdint.h>
#include "nty_config.h"

// epoll 事件类型
typedef enum
{

	NTY_EPOLLNONE = 0x0000,
	NTY_EPOLLIN = 0x0001,
	NTY_EPOLLPRI = 0x0002,
	NTY_EPOLLOUT = 0x0004,
	NTY_EPOLLRDNORM = 0x0040,
	NTY_EPOLLRDBAND = 0x0080,
	NTY_EPOLLWRNORM = 0x0100,
	NTY_EPOLLWRBAND = 0x0200,
	NTY_EPOLLMSG = 0x0400,
	NTY_EPOLLERR = 0x0008,
	NTY_EPOLLHUP = 0x0010,
	NTY_EPOLLRDHUP = 0x2000,
	NTY_EPOLLONESHOT = (1 << 30),
	NTY_EPOLLET = (1 << 31)

} nty_epoll_type;

typedef enum
{
	NTY_EPOLL_CTL_ADD = 1,
	NTY_EPOLL_CTL_DEL = 2,
	NTY_EPOLL_CTL_MOD = 3,
} nty_epoll_op;

typedef union _nty_epoll_data {
	void *ptr;
	int sockid;
	uint32_t u32;
	uint64_t u64;
} nty_epoll_data;

typedef struct
{
	uint32_t events;
	uint64_t data;
} nty_epoll_event;

int nty_epoll_create(int size);
int nty_epoll_ctl(int epid, int op, int sockid, nty_epoll_event *event);
int nty_epoll_wait(int epid, nty_epoll_event *events, int maxevents, int timeout);

#if NTY_ENABLE_EPOLL_RB

enum EPOLL_EVENTS
{
	EPOLLNONE = 0x0000,
	EPOLLIN = 0x0001,
	EPOLLPRI = 0x0002,
	EPOLLOUT = 0x0004,
	EPOLLRDNORM = 0x0040,
	EPOLLRDBAND = 0x0080,
	EPOLLWRNORM = 0x0100,
	EPOLLWRBAND = 0x0200,
	EPOLLMSG = 0x0400,
	EPOLLERR = 0x0008,
	EPOLLHUP = 0x0010,
	EPOLLRDHUP = 0x2000,
	EPOLLONESHOT = (1 << 30),
	EPOLLET = (1 << 31)

};

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

typedef union epoll_data {
	void *ptr;
	int fd;
	uint32_t u32;
	uint64_t u64;
} epoll_data_t;

struct epoll_event
{
	uint32_t events;
	epoll_data_t data;
};

/**
 * @function	创建 epoll 对象，创建一颗空的红黑树，一个空双向链表。
 * @paras	size	无用，> 0 即可。
 * @ret	>0	socket 描述符的值 。
 * 		-1	申请内存失败。
 * 		-2	初始化互斥量和信号量失败。
*/
int epoll_create(int size);

/**
 * @function	向红黑树中增加/删除/修改 tcp 连接以及相关事件。
 * @paras	epid	eventpoll 文件描述符，由 epoll_create 返回。
 * 			op	对 sockid 进行的操作的集合。
 * 			sockid	被操作的连接。
 * 			event	保存需要监控的事件集合以及用户的其他数据。
 * @ret  0   操作成功
 *       -1  操作失败。
 * @notice  该函数主要完成的就是在 eventpoll 中对红黑树进行增删改的操作。
*/
int epoll_ctl(int epid, int op, int sockid, struct epoll_event *event);

/**
 * @function	从双向链表中获取最大数量为 maxevents 的 epoll_event ，并保存在 events 数组中。
 * @paras	epid	eventpoll 文件描述符，由 epoll_create 返回。
 * 			events	数组，保存已经发生指定的事件的 epoll_event 。
 * 			maxevents	events 数组大小。
 * 			timeout	超时时间，单位 ms 。
 * @ret	> 0	返回有多少个被监控的事件发生了，这些事件对应的 epoll_event 放在了 events 数组中。
 * 		= 0	没有被监控的事件发生，有可能超时了。
 * 		-1	发生了错误。
*/
int epoll_wait(int epid, struct epoll_event *events, int maxevents, int timeout);

int nty_epoll_close_socket(int epid);

#endif

#endif

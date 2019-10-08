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

#include "nty_tree.h"
#include "nty_queue.h"
#include "nty_epoll_inner.h"
#include "nty_config.h"

#if NTY_ENABLE_EPOLL_RB

#include <pthread.h>
#include <stdint.h>
#include <time.h>

//static pthread_mutex_t epmutex;

extern nty_tcp_manager *nty_get_tcp_manager(void);

int epoll_create(int size)
{

    if (size <= 0)
        return -1;

    nty_tcp_manager *tcp = nty_get_tcp_manager();
    if (!tcp)
        return -1;

    struct _nty_socket *epsocket = nty_socket_allocate(NTY_TCP_SOCK_EPOLL);
    if (epsocket == NULL)
    {
        nty_trace_epoll("malloc failed\n");
        return -1;
    }

    /**
	 * 创建 epoll 对象。
	*/
    struct eventpoll *ep = (struct eventpoll *)calloc(1, sizeof(struct eventpoll));
    if (!ep)
    {
        nty_free_socket(epsocket->id, 0);
        return -1;
    }

    ep->rbcnt = 0;

    /**
	 * 红黑树根节点指向 nullptr 。
	 * ep->rbr.rbh_root = NULL 
	*/
    RB_INIT(&ep->rbr);

    /**
	 * 双向链表的根节点指向 nullptr 。
	 * ep->rdlist->lh_first = NULL
	*/
    LIST_INIT(&ep->rdlist);

    if (pthread_mutex_init(&ep->mtx, NULL))
    {
        free(ep);
        nty_free_socket(epsocket->id, 0);
        return -2;
    }

    if (pthread_mutex_init(&ep->cdmtx, NULL))
    {
        pthread_mutex_destroy(&ep->mtx);
        free(ep);
        nty_free_socket(epsocket->id, 0);
        return -2;
    }

    if (pthread_cond_init(&ep->cond, NULL))
    {
        pthread_mutex_destroy(&ep->cdmtx);
        pthread_mutex_destroy(&ep->mtx);
        free(ep);
        nty_free_socket(epsocket->id, 0);
        return -2;
    }

    if (pthread_spin_init(&ep->lock, PTHREAD_PROCESS_SHARED))
    {
        pthread_cond_destroy(&ep->cond);
        pthread_mutex_destroy(&ep->cdmtx);
        pthread_mutex_destroy(&ep->mtx);
        free(ep);

        nty_free_socket(epsocket->id, 0);
        return -2;
    }

    tcp->ep = (void *)ep;
    epsocket->ep = (void *)ep;

    return epsocket->id;
}

int epoll_ctl(int epid, int op, int sockid, struct epoll_event *event)
{

    nty_tcp_manager *tcp = nty_get_tcp_manager();
    if (!tcp)
        return -1;

    struct _nty_socket *epsocket = tcp->fdtable->sockfds[epid];
    //struct _nty_socket *socket = tcp->fdtable->sockfds[sockid];

    if (epsocket->socktype == NTY_TCP_SOCK_UNUSED)
    {
        errno = -EBADF;
        return -1;
    }

    if (epsocket->socktype != NTY_TCP_SOCK_EPOLL)
    {
        errno = -EINVAL;
        return -1;
    }

    nty_trace_epoll(" epoll_ctl --> eventpoll\n");

    struct eventpoll *ep = (struct eventpoll *)epsocket->ep;
    if (!ep || (!event && op != EPOLL_CTL_DEL))
    {
        errno = -EINVAL;
        return -1;
    }
    /**
	 * （1）在 sockid 上添加关联的事件。 
	*/
    if (op == EPOLL_CTL_ADD)
    {
        pthread_mutex_lock(&ep->mtx);

        struct epitem tmp;
        tmp.sockfd = sockid;
        /**
         * _epoll_rb_socket_RB_FIND(&ep->rbr, &tmp)
         * 确定当前 eventpoll 中的红黑树中是否存在该形参指定的 socket 描述符。
        */
        struct epitem *epi = RB_FIND(_epoll_rb_socket, &ep->rbr, &tmp);
        if (epi)
        {
            /**
			 * 原来有的节点不能再次插入。
			*/
            nty_trace_epoll("rbtree is exist\n");
            pthread_mutex_unlock(&ep->mtx);
            return -1;
        }

        /**
		 * 走到这里说明 eventpoll 中的红黑树中的所有节点均没有该 socket 描述符。
		*/
        /**
		 * a、创建一个 epitem 对象，该对象就是红黑树的一个节点。
		*/
        epi = (struct epitem *)calloc(1, sizeof(struct epitem));
        if (!epi)
        {
            pthread_mutex_unlock(&ep->mtx);
            errno = -ENOMEM;
            return -1;
        }
        
        /**
		 * b、把 socket 保存到该节点中。
		*/
        epi->sockfd = sockid;

        /**
		 * c、将要监控的事件以及其他变量保存到该节点中。
		*/
        memcpy(&epi->event, event, sizeof(struct epoll_event));

        /**
		 * d、将这个节点插入到红黑树中。
		*/
        epi = RB_INSERT(_epoll_rb_socket, &ep->rbr, epi);
        assert(epi == NULL);
        ep->rbcnt++;

        pthread_mutex_unlock(&ep->mtx);
    }

    /**
	 * （2）将节点从红黑树中删除。
	*/
    else if (op == EPOLL_CTL_DEL)
    {
        pthread_mutex_lock(&ep->mtx);

        struct epitem tmp;
        tmp.sockfd = sockid;
        struct epitem *epi = RB_FIND(_epoll_rb_socket, &ep->rbr, &tmp);
        if (!epi)
        {
            nty_trace_epoll("rbtree no exist\n");
            pthread_mutex_unlock(&ep->mtx);
            return -1;
        }
        /**
		 * 只有在红黑树中找到该节点才能走到这里。
		*/
        /**
		 * 从红黑树中把节点移除。
		*/
        epi = RB_REMOVE(_epoll_rb_socket, &ep->rbr, epi);
        if (!epi)
        {
            nty_trace_epoll("rbtree is no exist\n");
            pthread_mutex_unlock(&ep->mtx);
            return -1;
        }

        ep->rbcnt--;
        free(epi);

        pthread_mutex_unlock(&ep->mtx);
    }

    /**
	 * （3）修改红黑树某个节点的内容。
	*/
    else if (op == EPOLL_CTL_MOD)
    {
        struct epitem tmp;
        tmp.sockfd = sockid;
        struct epitem *epi = RB_FIND(_epoll_rb_socket, &ep->rbr, &tmp);
        if (epi)
        {
            epi->event.events = event->events;
            epi->event.events |= EPOLLERR | EPOLLHUP;
        }
        else
        {
            errno = -ENOENT;
            return -1;
        }
    }
    else
    {
        nty_trace_epoll("op is no exist\n");
        assert(0);
    }

    return 0;
}

int epoll_wait(int epid, struct epoll_event *events, int maxevents, int timeout)
{

    nty_tcp_manager *tcp = nty_get_tcp_manager();
    if (!tcp)
        return -1;

    //nty_socket_map *epsocket = &tcp->smap[epid];
    struct _nty_socket *epsocket = tcp->fdtable->sockfds[epid];
    if (epsocket == NULL)
        return -1;

    if (epsocket->socktype == NTY_TCP_SOCK_UNUSED)
    {
        errno = -EBADF;
        return -1;
    }

    if (epsocket->socktype != NTY_TCP_SOCK_EPOLL)
    {
        errno = -EINVAL;
        return -1;
    }

    struct eventpoll *ep = (struct eventpoll *)epsocket->ep;
    if (!ep || !events || maxevents <= 0)
    {
        errno = -EINVAL;
        return -1;
    }

    if (pthread_mutex_lock(&ep->cdmtx))
    {
        if (errno == EDEADLK)
        {
            nty_trace_epoll("epoll lock blocked\n");
        }
        assert(0);
    }

    /**
	 * 这个 while 用来等待一定的时间。
	 * 在这段时间内，发生事件的TCP连接，相关的节点，
	 * 会被操作系统扔到双向链表去，当然这个节点同时也在红黑树中。
	*/
    while (ep->rdnum == 0 && timeout != 0)
    {

        ep->waiting = 1;
        if (timeout > 0)
        {

            struct timespec deadline;

            clock_gettime(CLOCK_REALTIME, &deadline);
            if (timeout >= 1000)
            {
                int sec;
                sec = timeout / 1000;
                deadline.tv_sec += sec;
                timeout -= sec * 1000;
            }

            deadline.tv_nsec += timeout * 1000000;

            if (deadline.tv_nsec >= 1000000000)
            {
                deadline.tv_sec++;
                deadline.tv_nsec -= 1000000000;
            }

            int ret = pthread_cond_timedwait(&ep->cond, &ep->cdmtx, &deadline);
            if (ret && ret != ETIMEDOUT)
            {
                nty_trace_epoll("pthread_cond_timewait\n");

                pthread_mutex_unlock(&ep->cdmtx);

                return -1;
            }
            timeout = 0;
        }
        else if (timeout < 0)
        {

            int ret = pthread_cond_wait(&ep->cond, &ep->cdmtx);
            if (ret)
            {
                nty_trace_epoll("pthread_cond_wait\n");
                pthread_mutex_unlock(&ep->cdmtx);

                return -1;
            }
        }
        ep->waiting = 0;
    }

    pthread_mutex_unlock(&ep->cdmtx);

    /**
	 * 等一小段时间，等时间到达后，流程来到这里。
	*/
    pthread_spin_lock(&ep->lock);

    int cnt = 0;
    /**
	 * a、取得事件的数量。
	 * ep->rdnum，代表双向链表里边的节点数量，也就是有多少个tcp连接来事件了。
	 * maxevents，这次调用最多可以收集的已经就绪的读写事件的数量。
	*/
    int num = (ep->rdnum > maxevents ? maxevents : ep->rdnum);
    int i = 0;

    while (num != 0 && !LIST_EMPTY(&ep->rdlist))
    {
        /**
		 * b、每次都从双向链表中取一个节点。
		*/
        struct epitem *epi = LIST_FIRST(&ep->rdlist);
        /**
		 * c、把这个节点从双向链表中删除。
		 * 但不影响该节点依旧在红黑树中。
		*/
        LIST_REMOVE(epi, rdlink);

        /**
		 * d、标记该节点已经不在双向链表中了。
		*/
        epi->rdy = 0;

        /**
		 * 
		 * e、将事件标记信息拷贝到提供的 events 参数中。
		*/
        memcpy(&events[i++], &epi->event, sizeof(struct epoll_event));

        num--;
        cnt++;
        ep->rdnum--;
    }

    pthread_spin_unlock(&ep->lock);

    /**
	 * f、返回实际发生事件的tcp连接的数量。
	*/
    return cnt;
}

int epoll_event_callback(struct eventpoll *ep, int sockid, uint32_t event)
{

    struct epitem tmp;
    tmp.sockfd = sockid;
    /**
	 * （1）根据 socket 找到红黑树中该 socket 对应的节点。
	*/
    struct epitem *epi = RB_FIND(_epoll_rb_socket, &ep->rbr, &tmp);
    if (!epi)
    {
        nty_trace_epoll("rbtree not exist\n");
        assert(0);
    }

    /**
	 * （2）判断该节点是否已经在双向链表中。
	*/
    if (epi->rdy)
    {
        /**
		 * 若该节点在双向链表中，则将新发生的事件插入到现有的时间标记中。
		*/
        epi->event.events |= event;
        return 1;
    }

    /**
	 * 走到这里说明该节点没有在双向链表中，需要将该节点插入到双向链表中。
	*/
    nty_trace_epoll("epoll_event_callback --> %d\n", epi->sockfd);

    pthread_spin_lock(&ep->lock);

    /**
	 * （3）标记该节点已经插入到双向链表中了。
	 * 在 epoll_wait 中，该标志会被置为 0 。
	*/
    epi->rdy = 1;

    /**
	 * （4）将该节点插入到双向链表的表头位置。
	*/
    LIST_INSERT_HEAD(&ep->rdlist, epi, rdlink);

    /**
	 * （5）双向链表中的节点数量 + 1 。
	 * 该值在 epoll_wait 中会被 - 1 。
	*/
    ep->rdnum++;
    pthread_spin_unlock(&ep->lock);

    pthread_mutex_lock(&ep->cdmtx);

    pthread_cond_signal(&ep->cond);
    pthread_mutex_unlock(&ep->cdmtx);
    return 0;
}

static int epoll_destroy(struct eventpoll *ep)
{

    //remove rdlist

    while (!LIST_EMPTY(&ep->rdlist))
    {
        struct epitem *epi = LIST_FIRST(&ep->rdlist);
        LIST_REMOVE(epi, rdlink);
    }

    //remove rbtree
    pthread_mutex_lock(&ep->mtx);

    for (;;)
    {
        struct epitem *epi = RB_MIN(_epoll_rb_socket, &ep->rbr);
        if (epi == NULL)
            break;

        epi = RB_REMOVE(_epoll_rb_socket, &ep->rbr, epi);
        free(epi);
    }
    pthread_mutex_unlock(&ep->mtx);

    return 0;
}

int nty_epoll_close_socket(int epid)
{

    nty_tcp_manager *tcp = nty_get_tcp_manager();
    if (!tcp)
        return -1;

    struct eventpoll *ep = (struct eventpoll *)tcp->fdtable->sockfds[epid]->ep;
    if (!ep)
    {
        errno = -EINVAL;
        return -1;
    }

    epoll_destroy(ep);

    pthread_mutex_lock(&ep->mtx);
    tcp->ep = NULL;
    tcp->fdtable->sockfds[epid]->ep = NULL;
    pthread_cond_signal(&ep->cond);
    pthread_mutex_unlock(&ep->mtx);

    pthread_cond_destroy(&ep->cond);
    pthread_mutex_destroy(&ep->mtx);

    pthread_spin_destroy(&ep->lock);

    free(ep);

    return 0;
}

#endif

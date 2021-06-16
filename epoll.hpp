/* epoll.hpp -- v1.0 -- encapsulated epoll instance
   Author: Sam Y. 2021 */

#ifndef _COMM_EPOLL_HPP
#define _COMM_EPOLL_HPP

#include <sys/epoll.h>

#include "client.hpp"
#include "endpoint.hpp"

namespace comm {

    // Fwd. decl.
    class client_pool_base;
    class server_pool_base;

    namespace detail {
        /*! Helper, implements epoll_ctl()
         */
        inline int ctl(const int epfd,
                       const int opcode,
                       const int sfd,
                       const int events,
                       void* ptr)
        {
            ::epoll_event epollEvent = {  };
            epollEvent.events = events;
            epollEvent.data.ptr = ptr;

            const int ret = epoll_ctl(epfd, opcode, sfd, &epollEvent);
            return ret;
        }

        /*! Helper, implements epoll_ctl()
         */
        inline int ctl(const int epfd,
                       const int opcode,
                       const int sfd,
                       const int events,
                       const int userdata)
        {
            ::epoll_event epollEvent = {  };
            epollEvent.events = events;
            epollEvent.data.u32 = userdata;

            const int ret = epoll_ctl(epfd, opcode, sfd, &epollEvent);
            return ret;
        }
    }

    //! @class epollen
    /*! encapsulates an epoll instance
     */
    template <typename Tderiv>
    struct epoll {
    public:

        //! dtor.
        //!
        ~epoll() {
            endpoint_close(epfd_);
            endpoint_close(selfpipe_[0]);
            endpoint_close(selfpipe_[1]);
        }

        //! ctor.
        //! @param maxevents    maximum number of epoll to read before calling event handler
        //!
        epoll(const int maxevents = DEFAULT_MAX_EVENTS) : maxevents_(maxevents) {

            // Generate epoll instance
            if ((epfd_ = epoll_create1(0)) == -1) {
                throw std::runtime_error("failed to create epoll descriptor");
            }

            // Generate the self-pipe used to send control signals; signals close
            if (::socketpair(AF_UNIX, SOCK_STREAM, 0, selfpipe_) == -1) {
                throw std::runtime_error("failed to create epoll descriptor");
            }

            else
            {
                if (detail::ctl(epfd_,
                                EPOLL_CTL_ADD,
                                selfpipe_[1],
                                EPOLLIN | EPOLLET | EPOLLONESHOT,
                                nullptr) == -1) {
                    throw std::runtime_error("failed to create epoll descriptor");
                }
            }
        }

        //! Removes managed socket
        //! @param sfd    socket file descriptor
        int remove(const int sfd) {
            struct epoll_event event = {  };
            const int ret = epoll_ctl(epfd_, EPOLL_CTL_DEL, sfd, &event);
            return ret;
        }

        //! Adds managed server socket
        //! @param sfd    socket file descriptor
        template <typename Q = Tderiv>
        typename std::enable_if<std::is_base_of<server_pool_base, Q>::value,
                                int>::type add(int sfd) {
            const int events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
            const int ret = detail::ctl(epfd_, EPOLL_CTL_ADD, sfd, events, sfd);
            return ret;
        }

        //! Adds managed client descriptor
        //! @param handler    pointer to client
        template <typename Q = Tderiv>
        typename std::enable_if<std::is_base_of<client_pool_base, Q>::value,
                                int>::type add(client* handler) {
            const int events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLPRI | EPOLLONESHOT;
            const int ret = detail::ctl(epfd_, EPOLL_CTL_ADD, handler->sfd, events, handler);
            return ret;
        }

        //! Re-adds client descriptor
        //! @param handler    pointer to client
        template <typename Q = Tderiv>
        typename std::enable_if<std::is_base_of<client_pool_base, Q>::value,
                                int>::type rearm(client* handler) {
            const int events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLPRI | EPOLLONESHOT;
            const int ret = detail::ctl(epfd_, EPOLL_CTL_MOD, handler->sfd, events, handler);
            return ret;
        }

        //! Waits on epoll instance
        //!
        inline void wait();

        //! Signals shut down by writing to pipe
        //!
        void close() {
            char ch = '$';
            endpoint_write(selfpipe_[0], &ch, sizeof(ch));
        }

    private:

        static const int DEFAULT_MAX_EVENTS = 65536;

        // Pipe used to send control signals; signals close
        int selfpipe_[2];
        // Epoll parameters
        int epfd_, maxevents_;

        // Non-copyable object
        explicit epoll(epoll&) = delete;
        explicit epoll(const epoll&) = delete;
    };

    /*! Waits on epoll instance
     */
    template <typename Tderiv>
    void comm::epoll<Tderiv>::wait()
    {
        const int epfd = epfd_;
        const int maxevents = maxevents_;

        epoll_event* const events = new epoll_event[maxevents];

        while (true)
        {
            int nevents;
            if ((nevents = epoll_wait(epfd, events, maxevents, 0)) == -1) {
                break; // Encountered error
            }

            for (int i = 0; i != nevents; ++i)
            {
                // If have a control socket, process message
                if (events[i].data.ptr == nullptr)
                {
                    delete[] events;

                    char ch;
                    endpoint_read(selfpipe_[1], &ch, sizeof(ch));

                    // Daisy-chained shutdown
                    // This routine will shut down the next thread to call epoll_wait()
                    if (detail::ctl(epfd_, EPOLL_CTL_MOD, selfpipe_[1], EPOLLIN | EPOLLET | EPOLLONESHOT,
                                    nullptr) == -1) {
                        throw std::runtime_error("failed to create epoll descriptor");
                    }

                    else
                    {
                        char ch = '$';
                        endpoint_write(selfpipe_[0], &ch, sizeof(ch));
                    }

                    return;
                }

                // Otherwise, have a regular socket, so handle the event
                else
                {
                    static_cast<Tderiv*>(this)->process(static_cast<Tderiv*>(this)->cast(events[i].data),
                                                        events[i].events);
                }
            }
        }
    }
}

#endif

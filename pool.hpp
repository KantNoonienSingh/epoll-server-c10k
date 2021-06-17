/* pool.hpp -- v1.0 -- client and server event handlers
   Author: Sam Y. 2021 */

#ifndef _COMM_POOL_HPP
#define _COMM_POOL_HPP

#include <mutex>
#include <thread>
#include <vector>

#include <sys/ioctl.h>

#include "atomic_queue.hpp"
#include "epoll.hpp"

namespace comm {

    class client_pool_base {  };
    class server_pool_base {  };

    //! @class client_pool
    /*! Encapsulates event handling for multiple clients
     */
    template <typename Tderiv>
    class client_pool : public client_pool_base,
                        public epoll<client_pool<Tderiv> > {
    public:

        //! dtor.
        //
        ~client_pool() {

            std::lock_guard<std::mutex> lock(lock_);

            unused_.destroy();
            del_memmap<client>(mem_, clientcap_);
        }

        //! ctor.
        //! @param nworkers     client handler thread count
        //! @param clientcap    maximum number of clients
        client_pool(const std::size_t nworkers, std::size_t clientcap) : nworkers_(nworkers)
                                                                       , mem_(gen_memmap<client>(&clientcap))
                                                                       , clientcap_(clientcap)
                                                                       , clientsize_(0)
                                                                       , unused_(clientcap) {

            client** data = unused_.data();

            std::size_t i = 0;
            for ( ; i != clientcap; ++i)
                data[i] = &mem_[i];
        }

        //! Adds a new client
        //! @param sfd    file descriptor
        bool add_client(const int sfd) {

            // Ensure that we haven't exceeded client capacity
            if (clientsize_.load() == clientcap_)
                return false;

            client* const cl = use(sfd);
            const int ret = epoll<client_pool>::add(cl);
            return ret == 0;
        }

        //! Starts instance
        //!
        void run() {

            std::lock_guard<std::mutex> lock(lock_);

            if (threads_.empty())
            {
                for (std::size_t i = 0; i != nworkers_; ++i)
                {
                    threads_.emplace_back([this] {
                        epoll<client_pool<Tderiv> >::wait();
                    });
                }
            }
        }

        //! Stops running instance
        //!
        void stop() {

            std::lock_guard<std::mutex> lock(lock_);

            if (threads_.empty())
                return; // Nothing to do

            epoll<client_pool<Tderiv> >::close();

            for (std::size_t i = 0; i != threads_.size(); ++i)
                threads_[i].join();

            for (std::size_t i = 0; i != clientcap_; ++i)
            {
                if (mem_->sfd)
                    endpoint_close(mem_->sfd);
            }
        }

        //! Override this to handle out-of-band events
        //! @param sfd        triggered file descriptor
        //! @param oobdata    oob byte
        inline void on_oob(int sfd, char oobdata) {
            (void)sfd;
            (void)oobdata;
        }

        //! Override to handle input events
        //! @param sfd        triggered file descriptor
        //! @param data       received data
        //! @param datalen    received data length
        inline void on_input(int sfd, char* data, int datalen) {
            (void)sfd;
            (void)data;
            (void)datalen;
        }

        //! Override this to handle output-ready events
        //! @param sfd    triggered file descriptor
        inline void on_write_ready(int sfd) {
            (void)sfd;
        }

    private:

        friend epoll<client_pool<Tderiv> >;

        // Applied to critical section when starting and stopping the running instance
        std::mutex               lock_;

        std::vector<std::thread> threads_;
        std::size_t              nworkers_;

        // Allocated slab of memory, maximum client size
        client*                  mem_;

        std::size_t              clientcap_;
        std::atomic<std::size_t> clientsize_;

        // Pointers to currently unused clients
        atomic_queue<client*>    unused_;

        /*! Called on epoll event, casts epoll data value to correct type before passing it to process()
         */
        client* cast(epoll_data data) {
            return static_cast<client*>(data.ptr);
        }

        /*! Called on epoll event to processes triggered file descriptor
         */
        inline void process(client* const cl, const int flags);

        /*! Closes socket and stores client to unused queue
         */
        void unuse(client* const cl) {

            epoll<client_pool>::remove(cl->sfd);
            endpoint_close(cl->sfd);
            cl->sfd = 0;
            unused_.enqueue(cl);
            --clientsize_;
        }

        /*! Allocates new client
         */
        client* use(const int sfd) {

            ++clientsize_;
            void* mem = unused_.dequeue();
            return new (mem) client(sfd);
        }

        /*! EPOLLOUT
         */
        inline void handle_epollout(client* const);
        /*! EPOLLIN
         */
        inline void handle_epollin(client* const);
        /*! EPOLLPRI
         */
        inline void handle_epollpri(client* const);
    };


    /*! Processes epoll events
     */
    template <typename Tderiv>
    void client_pool<Tderiv>::process(client* const client, int flags)
    {
        switch (flags)
        {
            case EPOLLHUP:
            case EPOLLHUP | EPOLLOUT:

            case EPOLLRDHUP:
            case EPOLLRDHUP | EPOLLOUT: // > ignore EPOLLOUT
            {
                unuse(client);
                break;
            }

            case EPOLLIN:
            case EPOLLIN | EPOLLHUP:
            case EPOLLIN | EPOLLHUP | EPOLLOUT: // > ignore EPOLLOUT

            case EPOLLIN | EPOLLRDHUP:
            case EPOLLIN | EPOLLRDHUP | EPOLLOUT: // > ignore EPOLLOUT
            {
                handle_epollin(client); // > Also processes hangup (on 0-byte read)
                break;
            }

            case EPOLLPRI:
            case EPOLLPRI | EPOLLHUP:
            case EPOLLPRI | EPOLLHUP | EPOLLOUT: // > ignore EPOLLOUT

            case EPOLLPRI | EPOLLRDHUP:
            case EPOLLPRI | EPOLLRDHUP | EPOLLOUT: // > ignore EPOLLOUT
            {
                handle_epollpri(client); // > Also process hangup (on 0-byte read)
                break;
            }

            case EPOLLIN | EPOLLPRI:
            case EPOLLIN | EPOLLPRI | EPOLLHUP:
            case EPOLLIN | EPOLLPRI | EPOLLHUP | EPOLLOUT: // > ignore EPOLLOUT

            case EPOLLIN | EPOLLPRI | EPOLLRDHUP:
            case EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLOUT: // > ignore EPOLLOUT
            {
                handle_epollpri(client); // > Also processes hangup (on 0-byte read)
                break;
            }

            case EPOLLOUT:
            {
                handle_epollout(client);
                break;
            }

            case EPOLLIN | EPOLLOUT:
            {
                handle_epollin(client);
                handle_epollout(client);
                break;
            }

            case EPOLLPRI | EPOLLOUT:
            {
                handle_epollpri(client);
                handle_epollout(client);
                break;
            }

            case EPOLLIN | EPOLLPRI | EPOLLOUT:
            {
                handle_epollpri(client);
                handle_epollout(client);
                break;
            }

            default: {
                // Have error, close the socket
                if ((flags & EPOLLERR) == EPOLLERR) {
                    unuse(client);
                }
            }
        }
    }

    /*! EPOLLOUT
     */
    template <typename Tderiv>
    void client_pool<Tderiv>::handle_epollout(client* const cl)
    {
        static_cast<Tderiv*>(this)->on_write_ready(cl->sfd);
    }

    /*! EPOLLIN
     */
    template <typename Tderiv>
    void client_pool<Tderiv>::handle_epollin(client* const cl)
    {
        while (true)
        {
            int nbytes;
            switch (nbytes = endpoint_read(cl->sfd, cl->buff, static_cast<int>(cl->size)))
            {
                case -1:
                {
                    if (errno != EAGAIN)
                        unuse(cl); // Have actual error - done with client
                    else
                        epoll<client_pool>::rearm(cl);
                    return;
                }

                case 0:
                {
                    unuse(cl); // Disconnection - done with client
                    return;
                }

                // Have data to process...
                default:
                {
                    static_cast<Tderiv*>(this)->on_input(cl->sfd, cl->buff, nbytes);
                    break;
                }
            }
        }
    }

    /*! EPOLLPRI
     */
    template <typename Tderiv>
    void client_pool<Tderiv>::handle_epollpri(client* const cl)
    {
        while (true)
        {
            int mark;
            if (::ioctl(cl->sfd, SIOCATMARK, &mark) == -1)
            {
                unuse(cl); // > Have actual error - done with client
                break;
            }

            else
            {
                if (mark)
                {
                    char oobdata;
                    if (endpoint_read_oob(cl->sfd, &oobdata) != -1)
                        on_oob(cl->sfd, oobdata);

                    else
                    {
                        unuse(cl); // > Have actual error - done with client
                        break;
                    }
                }
            }

            int nbytes;
            switch ((nbytes = endpoint_read(cl->sfd, cl->buff, static_cast<int>(cl->size))))
            {
                case -1:
                {
                    if (errno != EAGAIN)
                        unuse(cl); // > Have actual error - done with client
                    else
                        epoll<client_pool>::rearm(cl);
                    return;
                }

                case 0:
                {
                    unuse(cl); // > Disconnection - done with client
                    return;
                }

                // Have data to process...
                default:
                {
                    static_cast<Tderiv*>(this)->on_input(cl->sfd, cl->buff, nbytes);
                    break;
                }
            }
        }
    }

    //! @class server_pool
    /*! Encapsulates event handling for multiple server sockets and their clients
     */
    template <typename T>
    class server_pool : public server_pool_base, public epoll<server_pool<T> > {
    public:

        //! ctor.
        //! @param nworkers     number of client handler thread
        //! @param clientcap    maximum number of clients
        server_pool(const std::size_t nworkers, const std::size_t clientcap) : clients_(nworkers, clientcap) {  }

        //! Starts listening on all server sockets
        //!
        void run() {

            std::lock_guard<std::mutex> lock(lock_);
            clients_.run();
            epoll<server_pool<T> >::wait();
        }

        //! Stops listening on all server sockets
        //!
        void stop() {

            epoll<server_pool<T> >::close();

            std::lock_guard<std::mutex> lock(lock_);
            clients_.stop();
        }

        //! Binds a listener socket to port
        //! @param port        port number
        //! @param queuelen    backlog queue length for accept()
        bool bind(const int port, const int queuelen) {

            int sfd;
            if ((sfd = comm::endpoint_tcp_server(port, queuelen)) == -1
                || comm::endpoint_unblock(sfd) == -1)
                return false;

            const int ret = epoll<server_pool<T> >::add(sfd);
            return ret == 0;
        }

        //! Adds a listener socket
        //! @param sfd    file descriptor
        bool add(const int sfd) {

            const int ret = epoll<server_pool<T> >::add(sfd);
            return ret == 0;
        }

    private:

        friend epoll<server_pool<T> >;

        /*! Called on epoll event, casts epoll data value to correct type before passing it to process()
         */
        int cast(epoll_data data) {
            return static_cast<int>(data.u32);
        }

        /*! Called on epoll event to handle connection requests
         */
        inline void process(const int sfd, const int flags);

        T clients_;
        std::mutex lock_;
    };

    /*! Called on epoll event to handle connection requests
     */
    template <typename T>
    void server_pool<T>::process(const int sfd, const int)
    {
        int cfd;
        while ((cfd = endpoint_accept(sfd)) != -1)
        {
            if (endpoint_unblock(cfd) != 0
                || !clients_.add_client(cfd)) {
                endpoint_close(cfd);
            }
        }
    }
}

#endif

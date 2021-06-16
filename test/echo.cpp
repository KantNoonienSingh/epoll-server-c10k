/* main.cpp -- v1.0 -- an echo server application
   Author: Sam Y. 2021 */

#include <cstring>
#include <memory>

#include "server.hpp"

namespace {

    /*! @class client packet handler
     */
    class echo : public comm::client_callback_handler<echo> {
    public:

        inline echo(const std::size_t nworkers,
                    const std::size_t size) : comm::client_callback_handler<echo>(nworkers, size) {  }

        inline void on_input(int sfd, char* data, int datalen) {
            // Just echo the message back
            comm::endpoint_write(sfd, data, datalen);
        }
    };
}

namespace {

    /*! Prints socket creation error to console 
     */
    void print_server_socket_error(const int port)
    {
        static const char* errstr = "Server socket creation error on port";

        int temp = port;
        int chcount = 0;

        do {
            ++chcount;
        } while(temp /= 10);

        char* buff = static_cast<char*>(std::calloc(::strlen(errstr) + chcount + 2, sizeof(char)));
        std::sprintf(buff, "%s %d", errstr, port);

        ::perror(buff);
        ::free(buff);
    }
}

/*! Entry point
 */
int main()
{
    const int port = 60008;
    const int maxclients = 2e5; // 200,000 max. connections
    const int nworkers = 10;

    int svfd;
    if ((svfd = comm::endpoint_tcp_server(port, 100000)) == -1 || comm::endpoint_unblock(svfd) == -1) {
        return print_server_socket_error(port), 1;
    }

    typedef comm::server<echo> server;

    std::shared_ptr<server> sv;

    try
    {
        // Initialise pool
        sv = std::make_shared<server>(nworkers, maxclients);

        if (!sv->add(svfd)) {
            return perror(""), 1;
        }
    }

    catch (std::runtime_error& e) {
        return perror(e.what()), 1;
    }

    // Start
    std::thread t1(&server::run, sv.get());

    // 'x' to quit
    int ch;
    do {
        ch = getchar();
    } while (tolower(ch) != 'x' && ch != EOF);

    // Stop server
    sv->stop();

    t1.join();
    comm::endpoint_close(svfd);

    return 0;
}

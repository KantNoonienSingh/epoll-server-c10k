/* client.hpp -- v1.0 -- the client data structure, containing file descriptor and pointer to buffer
   Author: Sam Y. 2021-22 */

#ifndef _COMM_CLIENT_HPP
#define _COMM_CLIENT_HPP

namespace comm {

    static const int MAX_READ_SIZE = 4096;

    //! @struct client
    /* remote connection endpoint
     */
    struct client {

        int sfd;

        static const int size = MAX_READ_SIZE;
        char buff[size + 1];

        explicit client(const int s) : sfd(s) {  }
    };
}

#endif

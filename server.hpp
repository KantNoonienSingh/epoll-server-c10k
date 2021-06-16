/* server.hpp -- v1.0 -- meta-include file, contains typedefs to important classes
   Author: Sam Y. 2021 */

#ifndef _COMM_SERVER_HPP
#define _COMM_SERVER_HPP

#include "pool.hpp"

namespace comm {

    template <typename T>
    using server = comm::server_pool<T>;

    template <typename T>
    using client_callback_handler = comm::client_pool<T>;
}

#endif

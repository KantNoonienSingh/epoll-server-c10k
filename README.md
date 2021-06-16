# Scalable Server
A scalable server written for the C10k problem
--------------------------------------------------------------------------------

This is a light-weight, header-only, epoll-based server library designed to achieve high TCP connection counts. Written in modern C++, the library uses a lock-free concurrency model for high performance thread synchronization. Worker threads are client-agnostic and have access to every client, ensuring that CPU resources aren't wasted on idling.

The is very much a work in progress and there is still more work to do. Please feel free to leave comments or suggestions.


Usage
--------------------------------------------------------------------------------
Below is example of an echo server (see also test/main.cpp):

<pre>
#include "server.hpp"

// The client packet handler
class echo : public comm::client_callback_handler<echo>
{
    ...
};

...

std::size_t j = 10;  // Maximum # of worker threads
std::size_t n = 2e5; // Maximum # of TCP connections at any one time. Any connection attempts past this threshold will be dropped.

// Initialize the server
typedef server<echo> server;
std::shared_ptr<server> sv;

try
{
    sv = std::make_shared<server>(j, n);
}

catch (comm_exception& e) 
{
    // Handle error
}

// The server must be bound to a port
int port60008 = 60008;
if (!sv->bind(port60007)) 
{
   // Handle error...
}

// The server can be bound to any number of additional ports
int port70011 = 700011;
if (!sv->bind(port70009))
{
    // Handle error...
}

// The server can also accept an existing open socket. 
// It is the user's responsibility to ensure that the socket is valid, bound to port, listening, and set to non-blocking (see O_NONBLOCK in the fcntl man page).
// It is valid to share this descriptor with other server instances.
if (!sv->add(existingSocket))
{
    // Handler error...
}

// Run the server in a separate thread
std::thread thr(&server::run, sv.get());

...

thr.join();
</pre>

Any custom packet handler must inherit from comm::client_pool, and can implement any of the following callbacks in order to receive event notifications:

<pre>
on_input(); // Invoked to process read
on_oob(); // Invoked to process out-of-band data
on_write_ready(); // Invoked when socket is ready to write
</pre>

Only the necessary callbacks need to be implemented. If the application doesn't need notification that the socket is ready to write, that event handler doesn't need to be implemented.

The server is edge triggered, meaning that it's the user's responsibility to process all events immediately. There will be no second notification and any unprocessed data will be discarded. Because they will be called from multiple threads, each callback must be fully re-entrant.

<pre>
// Example echo client handler implementation
class echo : public comm::client_pool<echo> 
{
    ...

public:

    void on_input(comm::client* cl) 
    {
        // Client socket
        int clientSock = comm::get_sock(cl);

        // Received message
        char* data = comm::get_data(cl);
        int dataLen = comm::get_data_len(cl);

        // Echo message back to the client
        comm::endpoint_write(clientSock, data, dataLen);
    }

    void on_oob(comm::client* cl, char oobFlag) 
    {
        ...
    }

    void on_write_ready(comm::client* cl) 
    {
        ...
    }
};
</pre>


Sources
--------------------------------------------------------------------------------
C10k problem\
<https://wikipedia.org/wiki/C10k_problem>


--------------------------------------------------------------------------------
This software is entirely in the public domain and is provided as is, without restricitions. See the LICENSE for more information.

No warranty implied; use at your own risk.

Copyright (c) 2021, Sam Y.
No Rights Reserved.

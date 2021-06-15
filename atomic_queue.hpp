/* atomic_queue.hpp -- v1.0 -- thread-safe circular queue with lock-free concurrency control
   Author: Sam Y. 2021 */

#ifndef _COMM_ATOMIC_QUEUE_HPP
#define _COMM_ATOMIC_QUEUE_HPP

#include <atomic>

#include "mem.hpp"

namespace comm {

    //! @class circular queue
    /*! thread-safe circular queue with lock-free concurrency control
     */
    template <typename T>
    class atomic_queue {
    public:

        //! ctor.
        //!
        atomic_queue() : buff_(nullptr), cap_(0) {

            ok_.store(false);
            head_.store(0);
            tail_.store(0);
        }

        //! ctor.
        //! @param capHint    queue will have *at least* this capacity,
        //!                   will be expanded up to page size border
        explicit atomic_queue(std::size_t capHint) {

            buff_ = static_cast<T*>(gen_memmap<T>(&capHint));
            cap_ = capHint;
            ok_.store(true);
            head_.store(0);
            tail_.store(0);
        }

        /*! Total capacity
         */
        std::size_t capacity() const {
            return cap_;
        }

        T* data() {
            return buff_;
        }

        const T* data() const {
            return buff_;
        }

        void destroy() {

            if (ok_.exchange(false)) {
                del_memmap<T>(buff_, cap_);
            }
        }

        /*! Pushes data to back
         */
        void enqueue(const T& data) {

            int t = tail_.fetch_add(1);
            buff_[t++] = data;

            // Maybe roll over
            if (cap_ <= t)
            {
                while (tail_.load() > t) {  }

                int r = t - cap_;
                if (tail_.compare_exchange_strong(t, t - cap_, std::memory_order_relaxed)) {
                    printf("CAP -> %d, TAIL -> %d -> %d\n", cap_, t, r);
                }
            }
        }

        /*! Pops data from front
         */
        T dequeue() {

            int h = head_.fetch_add(1);
            const T data = buff_[h++];

            // Maybe roll over
            if (cap_ <= h)
            {
                while (head_.load() > h) {  }

                int r = h - cap_;
                if (head_.compare_exchange_strong(h, h - cap_, std::memory_order_relaxed)) {
                    printf("CAP -> %d, HEAD -> %d -> %d\n", cap_, h, r);
                }
            }

            return data;
        }

    private:

        T* buff_;
        int cap_;

        std::atomic<bool> ok_;
        std::atomic<int> head_, tail_;
    };
}

#endif

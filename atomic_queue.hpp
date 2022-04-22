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
        atomic_queue() : buff_(nullptr), capacity_(0) {

            ok_.store(false);
            head_.store(0);
            tail_.store(0);
        }

        //! ctor.
        //! @param capacityHint    queue will have *at least* this capacity,
        //!                        will be expanded up to page size border
        explicit atomic_queue(std::size_t capacityHint) {

            buff_ = static_cast<T*>(gen_memmap<T>(&capacityHint));
            capacity_ = capacityHint;
            ok_.store(true);
            head_.store(0);
            tail_.store(0);
        }

        /*! Total capacity
         */
        std::size_t capacity() const {
            return capacity_;
        }

        T* data() {
            return buff_;
        }

        const T* data() const {
            return buff_;
        }

        void destroy() {

            if (ok_.exchange(false)) {
                del_memmap<T>(buff_, capacity_);
            }
        }

        /*! Pushes data to back
         */
        void enqueue(const T& data) {

            int t = tail_.fetch_add(1);
            buff_[t++] = data;

            // Maybe roll over
            if (capacity_ <= t)
            {
                while (tail_.load() > t) {  }
                tail_.compare_exchange_strong(t, t - capacity_, std::memory_order_relaxed);
            }
        }

        /*! Pops data from front
         */
        T dequeue() {

            int h = head_.fetch_add(1);
            const T data = buff_[h++];

            // Maybe roll over
            if (capacity_ <= h)
            {
                while (head_.load() > h) {  }
                head_.compare_exchange_strong(h, h - capacity_, std::memory_order_relaxed);
            }

            return data;
        }

    private:

        // Buffer
        T* buff_;
        // Buffer capacity
        int capacity_;

        // Destructor guard
        std::atomic<bool> ok_;
        // Circular queue pointer head and tail
        std::atomic<int> head_, tail_;
    };
}

#endif

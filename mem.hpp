/* pool.hpp -- v1.0 -- linux dual-page memmap allocation / deallocation
   Author: Sam Y. 2021 */

#ifndef _COMM_MEM_HPP
#define _COMM_MEM_HPP

#include <cstring>
#include <stdexcept>

#include <unistd.h>

#include <sys/mman.h>
#include <sys/syscall.h>

namespace comm {

    namespace detail {
        /*! Impl.
         */
        inline void* gen_memmap(const ::size_t unitsize,
                                std::size_t* count)
        {
            static const ::size_t pagesize = getpagesize();

            // Expand requested size to correct size (multiple of page size)
            if (*count % pagesize)
                *count = *count + pagesize - (*count % pagesize);

            // Correct number of bytes
            const ::size_t size = *count * unitsize;

            // Create anonymous file that resides in memory and set its size
            int fd;
            if ((fd = syscall(SYS_memfd_create, "anonymous", MFD_CLOEXEC)) == -1)
                throw std::runtime_error("memory allocation error");

            // Set file size equal to buffer size
            if (::ftruncate(fd, size) == -1)
            {
                ::close(fd);
                throw std::runtime_error("memory allocation error");
            }

            void* const page1 = ::mmap(nullptr, size * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            void* const page2 = reinterpret_cast<char*>(page1) + size;

            ::mmap(page1, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0); // 1st page of memory
            ::mmap(page2, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0); // 2nd page of memory
            return (::close(fd)), page1;
        }

        /*! Impl.
         */
        inline void del_memmap(void* tgt,
                               const ::size_t unitsize,
                               const ::size_t count)
        {
            const std::size_t size = count * unitsize;
            ::munmap(reinterpret_cast<char*>(tgt), size);
        }

        /*! Impl.
         */
        inline void mov_memmap(void* tgt,
                               void* src,
                               const std::size_t unitsize,
                               const std::size_t count)
        {
            static const ::size_t pagesize = getpagesize();
            std::size_t size = (count + pagesize - (count % pagesize)) * unitsize;

            ::memcpy(reinterpret_cast<char*>(tgt), reinterpret_cast<char*>(src), size);
            ::munmap(reinterpret_cast<char*>(src), size);
            ::munmap(reinterpret_cast<char*>(src) + size, size);
        }
    }

    //! Allocates a double-page memory map
    //! @param[in/out] sizeHint    memory map size will be *at least* this big,
    //!                            will be expanded up to page size border
    //! @return                    pointer to allocated memory map
    template <typename T>
    inline T* gen_memmap(std::size_t* const sizeHint)
    {
        return static_cast<T*>(detail::gen_memmap(sizeof(T), sizeHint));
    }

    //! Deallocates an existing memory map
    //! @param src     memory map
    //! @param size    memory map size
    template <typename T>
    inline void del_memmap(void* const src,
                           const std::size_t size)
    {
        detail::del_memmap(src, sizeof(T), size);
        detail::del_memmap(static_cast<T*>(src) + size, sizeof(T), size);
    }

    //! Reallocates memory map from source to target destination
    //! @param tgt     pointer to target
    //! @param src     pointer to source map
    //! @param size    memory map size
    template <typename T>
    inline void mov_memmap(void* const tgt,
                           void* const src,
                           const std::size_t size)
    {
        detail::mov_memmap(tgt, src, sizeof(T), size);
    }
}

#endif

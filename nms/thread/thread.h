#pragma once

#include <nms/core.h>

namespace nms::thread
{

class Thread final
{
public:
    template<class Tlambda>
    explicit Thread(Tlambda&& lambda) {
        auto stat = run(fwd<Tlambda>(lambda));
        (void)stat;
    }

    ~Thread() {
        if (impl_ != thrd_t(0)) {
            detach();
        }
    }

    Thread(Thread&& rhs) noexcept
        : impl_(move(rhs.impl_))
        , idx_(move(rhs.idx_)) {
        rhs.impl_ = thrd_t(0);
        rhs.idx_ = 0;
    }

    Thread(const Thread&) = delete;

    Thread& operator=(Thread&& rhs) noexcept {
        nms::swap(impl_, rhs.impl_);
        nms::swap(idx_,  rhs.idx_);
        return *this;
    }

    Thread& operator=(const Thread&) = delete;

    NMS_API int join();
    NMS_API int detach();

    NMS_API static void yield();
    NMS_API static int  sleep(double duration);

private:
    thrd_t  impl_   = thrd_t(0);
    i32     idx_    = 0;

#ifdef NMS_OS_UNIX
    using   thrd_ret_t = void*;
#else
    using   thrd_ret_t = void;
#endif

    NMS_API void  start(thrd_ret_t(*pfun)(void*), void* pobj);
    NMS_API static void* buff(size_t size);

    template<class Tlambda>
    bool run(Tlambda&& lambda) {
        auto addr = buff(sizeof(Tlambda));

        if (addr == nullptr) {
            return false;
        }
        auto pobj = new(addr)Tlambda(fwd<Tlambda>(lambda));

        auto pfun = [](void* ptr) -> thrd_ret_t {
            auto obj = static_cast<Tlambda*>(ptr);
            (*obj)();
            obj->~Tlambda();
#ifdef NMS_OS_UNIX
            return nullptr;
#endif
        };

        start(pfun, pobj);

        return true;
    }
};

inline void yield() {
    Thread::yield();
}

inline void sleep(double seconds) {
    Thread::sleep(seconds);
}

}

#ifndef MIO_VM_PROFILER_H_
#define MIO_VM_PROFILER_H_

#include "base.h"
#include "glog/logging.h"
#include <thread>
#include <atomic>

namespace mio {

class Profiler {
public:
    Profiler();
    ~Profiler();

    inline void Start();
    inline void Stop();

    DEF_PROP_RW(int, sample_rate)

    DISALLOW_IMPLICIT_CONSTRUCTORS(Profiler)
private:
    inline void DoSample();
    void SampleTick();

    int sample_rate_;
    std::thread *thread_ = nullptr;
    std::atomic<bool> shoud_sample_;
}; // class Profiler

inline void Profiler::Start() {
    DCHECK(thread_ == nullptr);
    shoud_sample_.store(true);
    thread_ = new std::thread([&]() {
        DoSample();
    });
}

inline void Profiler::Stop() {
    if (!thread_) {
        return;
    }
    shoud_sample_.store(false);
    DCHECK(thread_->joinable());
    thread_->join();
    delete thread_;
    thread_ = nullptr;
}

inline void Profiler::DoSample() {
    while (shoud_sample_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sample_rate_));
        SampleTick();
    }
}

} // namespace mio

#endif // MIO_VM_PROFILER_H_

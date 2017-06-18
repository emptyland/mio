#ifndef MIO_VM_PROFILER_H_
#define MIO_VM_PROFILER_H_

#include "base.h"
#include "glog/logging.h"
#include <thread>
#include <atomic>

namespace mio {

class MIOFunction;
class VM;

struct HotPointInformation {
    MIOFunction *fn;
    int64_t      sample_hit_count;
};

class Profiler {
public:
    Profiler(VM *vm, int max_hot_points);
    ~Profiler();

    inline void Start();
    void Stop();

    DEF_PROP_RW(int, sample_rate)
    DEF_PROP_RW(int, hit_count_threshold);

    inline int64_t EstimateCallHit(int64_t sample_hit) const;

    void TEST_PrintSamples();

    DISALLOW_IMPLICIT_CONSTRUCTORS(Profiler)
private:
    inline void DoSample();
    void SampleTick();

    VM *vm_;
    int sample_rate_ = 10;
    int max_hot_points_;
    int hit_count_threshold_ = 10000;
    std::thread *thread_ = nullptr;
    std::atomic<bool> shoud_sample_;
    HotPointInformation *hot_points_;
}; // class Profiler

inline void Profiler::Start() {
    DCHECK(thread_ == nullptr);
    shoud_sample_.store(true);
    thread_ = new std::thread([&]() {
        DoSample();
    });
}

inline void Profiler::DoSample() {
    while (shoud_sample_.load(std::memory_order_release)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sample_rate_));
        SampleTick();
    }
}

inline int64_t Profiler::EstimateCallHit(int64_t sample_hit) const {
    return sample_hit * (1000LL / sample_rate_);
}

} // namespace mio

#endif // MIO_VM_PROFILER_H_

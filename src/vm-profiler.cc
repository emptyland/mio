#include "vm-profiler.h"
#include "vm.h"
#include "vm-thread.h"
#include "vm-objects.h"

namespace mio {

Profiler::Profiler(VM *vm, int max_hot_points)
    : vm_(DCHECK_NOTNULL(vm))
    , max_hot_points_(max_hot_points) {
    DCHECK_GT(max_hot_points, 0);
    hot_points_ = new HotPointInformation[max_hot_points_];
    ::memset(hot_points_, 0, sizeof(HotPointInformation) * max_hot_points_);
}

Profiler::~Profiler() {
    Stop();
    for (int i = 0; i < max_hot_points_; ++i) {
        if (hot_points_[i].fn) {
            hot_points_[i].fn->Drop();
        }
    }
    delete[] hot_points_;
}

void Profiler::Stop() {
    if (!thread_) {
        return;
    }
    shoud_sample_.store(false, std::memory_order_acquire);
    DCHECK(thread_->joinable());
    thread_->join();

    delete thread_;
    thread_ = nullptr;
}

void Profiler::TEST_PrintSamples() {
    for (int i = 0; i < max_hot_points_; ++i) {
        auto fn = hot_points_[i].fn;
        if (!fn) {
            break;
        }
        if (fn->GetName()) {
            printf("[%02d] %s: %lld\n", i, fn->GetName()->GetData(), hot_points_[i].sample_hit_count);
        } else {
            printf("[%02d] %p: %lld\n", i, fn, hot_points_[i].sample_hit_count);
        }
    }
}

void Profiler::SampleTick() {
    if (vm_->current()->syscall() > 0) {
        return;
    }
    auto fn = make_handle(vm_->current()->callee());
    if (fn.empty()) {
        return;
    }
    if (fn->IsNativeFunction() ||
        (fn->IsClosure() && fn->AsClosure()->GetFunction()->IsNativeFunction())) {
        return;
    }

    auto pos = -1;
    for (int i = 0; i < max_hot_points_; ++i) {
        if (hot_points_[i].fn == nullptr ||
            hot_points_[i].fn == fn.get()) {
            pos = i;
            break;
        }
    }
    if (pos >= 0) {
        if (hot_points_[pos].fn == nullptr) {
            fn->Grab();
            hot_points_[pos].fn = fn.get();
            hot_points_[pos].sample_hit_count = 0;
        }
        hot_points_[pos].sample_hit_count++;
        if (pos != 0) {
            if (hot_points_[pos].sample_hit_count >
                hot_points_[pos - 1].sample_hit_count) {
                std::swap(hot_points_[pos], hot_points_[pos - 1]);
            }
        }
    } else {
        auto last = &hot_points_[max_hot_points_ - 1];
        last->fn->Drop();
        last->fn = fn.get();
        last->sample_hit_count = 1;
    }
}

} // namespace mio

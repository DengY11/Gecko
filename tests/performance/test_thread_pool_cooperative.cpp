#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <thread>
#include "thread_pool.hpp"

using namespace std::chrono_literals;

void test_cooperative_requeue_and_completion() {
    Gecko::ThreadPool pool(2, true, std::chrono::milliseconds(1));

    std::promise<void> done1;
    std::promise<void> done2;
    std::atomic<int> runs1{0};
    std::atomic<int> runs2{0};

    pool.enqueue_cooperative([&](Gecko::ThreadPool::TaskContext& ctx) {
        runs1++;
        /* Force yield on first invocation */
        if (runs1 == 1) {
            while (!ctx.should_yield()) {
                std::this_thread::sleep_for(200us);
            }
            return false; /* requeue */
        }
        done1.set_value();
        return true;
    }, Gecko::ThreadPool::TaskPriority::NORMAL, std::chrono::milliseconds(1));

    pool.enqueue_cooperative([&](Gecko::ThreadPool::TaskContext& ctx) {
        runs2++;
        if (runs2 < 3) {
            /* simulate work until time slice is consumed */
            std::this_thread::sleep_for(2ms);
            return false; /* requeue */
        }
        done2.set_value();
        return true;
    }, Gecko::ThreadPool::TaskPriority::HIGH, std::chrono::milliseconds(1));

    auto f1 = done1.get_future();
    auto f2 = done2.get_future();
    auto status1 = f1.wait_for(2s);
    auto status2 = f2.wait_for(2s);

    assert(status1 == std::future_status::ready);
    assert(status2 == std::future_status::ready);
    assert(runs1.load() >= 2);
    assert(runs2.load() >= 3);
}

int main() {
    test_cooperative_requeue_and_completion();
    return 0;
}

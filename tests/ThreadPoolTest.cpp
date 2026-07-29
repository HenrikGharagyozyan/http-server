#include <gtest/gtest.h>
#include "server/ThreadPool.hpp"

#include <atomic>
#include <chrono>
#include <latch>
#include <thread>
#include <vector>

TEST(ThreadPoolTest, ExecutesSingleTask)
{
    std::atomic<bool> ran{false};
    {
        server::ThreadPool pool(2);
        pool.enqueue([&] { ran = true; });
    } // Destructor drains the queue and joins

    EXPECT_TRUE(ran);
}

TEST(ThreadPoolTest, DestructorDrainsAllPendingTasks)
{
    constexpr int task_count = 200;
    std::atomic<int> counter{0};
    {
        server::ThreadPool pool(4);
        for (int i = 0; i < task_count; ++i)
        {
            pool.enqueue([&] { counter.fetch_add(1, std::memory_order_relaxed); });
        }
    }

    EXPECT_EQ(counter.load(), task_count);
}

TEST(ThreadPoolTest, TasksRunConcurrently)
{
    // Two tasks that each wait for the other: only completes if the pool
    // actually runs them on different threads at the same time.
    std::latch rendezvous(2);
    std::atomic<int> done{0};
    {
        server::ThreadPool pool(2);
        for (int i = 0; i < 2; ++i)
        {
            pool.enqueue([&]
            {
                rendezvous.arrive_and_wait();
                done.fetch_add(1);
            });
        }
    }

    EXPECT_EQ(done.load(), 2);
}

TEST(ThreadPoolTest, SingleThreadPoolRunsTasksInOrder)
{
    std::vector<int> order;
    {
        server::ThreadPool pool(1);
        for (int i = 0; i < 10; ++i)
        {
            pool.enqueue([&order, i] { order.push_back(i); });
        }
    }

    ASSERT_EQ(order.size(), 10u);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(order[static_cast<size_t>(i)], i);
    }
}

TEST(ThreadPoolTest, EnqueueFromWorkerThreadDoesNotDeadlock)
{
    std::atomic<bool> inner_ran{false};
    {
        server::ThreadPool pool(2);
        std::latch inner_done(1);
        pool.enqueue([&]
        {
            pool.enqueue([&]
            {
                inner_ran = true;
                inner_done.count_down();
            });
        });
        // Wait inside the scope so the inner task is enqueued before shutdown
        inner_done.wait();
    }

    EXPECT_TRUE(inner_ran);
}

TEST(ThreadPoolTest, DestructorReturnsWhileLongTaskAlreadyFinished)
{
    using namespace std::chrono_literals;

    std::atomic<bool> finished{false};
    auto start = std::chrono::steady_clock::now();
    {
        server::ThreadPool pool(2);
        pool.enqueue([&]
        {
            std::this_thread::sleep_for(50ms);
            finished = true;
        });
    }
    auto elapsed = std::chrono::steady_clock::now() - start;

    // The destructor must wait for the in-flight task
    EXPECT_TRUE(finished);
    EXPECT_GE(elapsed, 50ms);
}

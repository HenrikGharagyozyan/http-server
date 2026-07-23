#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace server 
{

    class ThreadPool
    {
    public:
        // Constructor starts the threads
        explicit ThreadPool(size_t num_threads);
        
        // Destructor stops the threads and waits for them to finish
        ~ThreadPool();

        // Method to add a task to the queue
        void enqueue(std::function<void()> task);

    private:
        std::vector<std::thread> workers_;           // Our workers
        std::queue<std::function<void()>> tasks_;    // Task queue

        std::mutex queue_mutex_;                     // Queue protection
        std::condition_variable condition_;          // Notifications for sleeping threads
        bool stop_;                                  // Pool stop flag
    };

} // namespace server
#include "server/ThreadPool.hpp"

namespace server
{

    ThreadPool::ThreadPool(size_t num_threads, size_t max_queue_size) 
        : stop_(false), max_queue_size_(max_queue_size)
    {
        // Start N threads when the pool is created
        for (size_t i = 0; i < num_threads; ++i) 
        {
            workers_.emplace_back([this] 
                {
                    while (true) 
                    {
                        std::function<void()> task;

                        // Critical section: lock the mutex to safely work with the queue
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex_);
                            
                            // The thread sleeps while the queue is empty and the pool is not stopping.
                            // When awakened, it checks this condition.
                            this->condition_.wait(lock, [this] 
                                {
                                    return this->stop_ || !this->tasks_.empty();
                                });

                            // If the pool is stopping and there are no more tasks, exit the thread
                            if (this->stop_ && this->tasks_.empty()) 
                            {
                                return;
                            }

                            // Take a task from the queue
                            task = std::move(this->tasks_.front());
                            this->tasks_.pop();
                        } // <--- Mutex is unlocked here!

                        // Execute the task.
                        // Important: we do this OUTSIDE the mutex so other threads
                        // can take the next tasks while this one is busy.
                        task();
                    }
                });
        }
    }

    bool ThreadPool::enqueue(std::function<void()> task) 
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // Backpressure: отбрасываем задачу, если очередь полна
            if (tasks_.size() >= max_queue_size_) 
            {
                return false;
            }
            tasks_.push(std::move(task));
        }
        condition_.notify_one(); // Wake one of the sleeping threads
        return true;
    }

    ThreadPool::~ThreadPool() 
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true; // Set the stop flag
        }
        condition_.notify_all(); // Wake all threads so they see the flag and exit

        // Wait until each thread finishes its current task and stops
        for (std::thread& worker : workers_) 
        {
            if (worker.joinable()) 
            {
                worker.join(); 
            }
        }
    }

} // namespace server
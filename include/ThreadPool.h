#pragma once
#include<iostream>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<atomic>
#include<queue>
#include<vector>
#include<stdexcept>
#include <utility>
#include <cstddef>

class ThreadPool
{
public:
    ThreadPool(std::size_t numThreads);
    ~ThreadPool();

    template<typename F,typename... Args>
    void enqueue(F&& f,Args&&... args)
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop)
            {
                throw std::runtime_error("线程池已停止");
            }
            auto task = [f = std::forward<F>(f), ...args = std::forward<Args>(args)]() mutable {
                f(args...);
            };
            tasks.emplace(std::move(task));
        }
        cv.notify_one();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> stop{false};
    void worker_loop();
};
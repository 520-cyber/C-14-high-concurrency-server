#include"ThreadPool.h"
ThreadPool::ThreadPool(std::size_t numThreads):stop(false){
    for(std::size_t i=0;i<numThreads;++i)
    {
        workers.emplace_back([this](){
        worker_loop();
    });
    }
}
ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex>lock(queue_mutex);
        stop=true;
    }
    cv.notify_all();
    for(std::thread& worker:workers){
        if(worker.joinable()){
            worker.join();
        }
    }
}
void ThreadPool::worker_loop()
{
    while(true){
        std::function<void()>task;
        {
            std::unique_lock<std::mutex>lock(queue_mutex);
            cv.wait(lock,[this](){
                return stop || !tasks.empty();
            });
            if(stop&&tasks.empty()){
                return;
            }
            task=std::move(tasks.front());
            tasks.pop();
        }
        task();
    }
}
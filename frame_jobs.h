#pragma once
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

// Persistent CPU workers. Every batch joins before graphics, input callbacks,
// model replacement or destruction. A model belongs to exactly one job.
class FrameJobs {
    std::vector<std::thread> workers;
    std::deque<std::function<void()>> queue;
    std::mutex mutex;
    std::condition_variable ready;
    bool stopping=false;
public:
    unsigned count()const{return unsigned(workers.size());}
    void ensure(unsigned count){
        while(workers.size()<count)workers.emplace_back([this]{
            for(;;){
                std::function<void()> job;
                {std::unique_lock lock(mutex);ready.wait(lock,[this]{return stopping||!queue.empty();});
                 if(stopping&&queue.empty())return;job=std::move(queue.front());queue.pop_front();}
                job();
            }
        });
    }
    void run(const std::vector<std::function<void()>>& jobs){
        if(jobs.size()<2){for(const auto& job:jobs)job();return;}
        ensure(std::min(unsigned(jobs.size()),std::max(1U,std::thread::hardware_concurrency()/2)));
        std::vector<std::future<void>> results;results.reserve(jobs.size());
        for(const auto& job:jobs){
            auto task=std::make_shared<std::packaged_task<void()>>(job);results.push_back(task->get_future());
            {std::lock_guard lock(mutex);queue.emplace_back([task]{(*task)();});}ready.notify_one();
        }
        std::exception_ptr failure;
        // Wait for the entire batch even if one actor fails, before freeing it.
        for(auto& result:results)try{result.get();}catch(...){if(!failure)failure=std::current_exception();}
        if(failure)std::rethrow_exception(failure);
    }
    void shutdown(){
        {std::lock_guard lock(mutex);stopping=true;}ready.notify_all();
        for(auto& worker:workers)if(worker.joinable())worker.join();workers.clear();
    }
    ~FrameJobs(){shutdown();}
};

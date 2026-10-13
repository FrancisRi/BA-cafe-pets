#include "frame_jobs.h"
#include <atomic>
#include <latch>
#include <stdexcept>
#include <iostream>
static void check(bool ok){if(!ok)throw std::runtime_error("CPU worker test failed");}
int main(){
    FrameJobs pool;pool.ensure(2);
    std::latch entered(2);std::atomic<unsigned> completed=0;
    const auto main=std::this_thread::get_id();
    pool.run({[&]{check(std::this_thread::get_id()!=main);entered.count_down();entered.wait();++completed;},
              [&]{check(std::this_thread::get_id()!=main);entered.count_down();entered.wait();++completed;}});
    check(completed==2);
    bool caught=false;
    try{pool.run({[]{throw std::runtime_error("actor failure");},[&]{++completed;}});}
    catch(const std::runtime_error&){caught=true;}
    check(caught&&completed==3); // Failed actor cannot free another running job.
    for(int frame=0;frame<100;frame++)pool.run({[&]{++completed;},[&]{++completed;},[&]{++completed;}});
    check(completed==303);
    pool.run({[&]{check(std::this_thread::get_id()==main);++completed;}});
    pool.shutdown();check(completed==304);
    std::cout<<"CPU worker overlap, exception barrier, reuse and shutdown passed\n";
}

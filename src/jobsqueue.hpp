#ifndef DDB_OWS_JOBSQUEUE_HPP
#define DDB_OWS_JOBSQUEUE_HPP

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include "job.hpp"

namespace ddb_ows {

class JobsQueue {
    private:
        std::deque<std::unique_ptr<Job>> q;
        std::condition_variable c;
        std::mutex m;
        bool isOpen;
    public:
        JobsQueue(void) :
            q(), c(), m()
        {
            isOpen = true;
        }
        void push(std::unique_ptr<Job> job);
        std::unique_ptr<Job> pop();
        void close();
        void open();
        void cancel();
        bool empty();
        int size();
};

}

#endif

#ifndef DDB_OWS_JOBSQUEUE_HPP
#define DDB_OWS_JOBSQUEUE_HPP

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>

#include "job.hpp"

namespace ddb_ows {

class JobsQueue {
  private:
    std::deque<std::unique_ptr<Job>> q;
    std::condition_variable c;
    std::mutex m;
    bool isOpen;

  public:
    JobsQueue(void) : q(), c(), m() { isOpen = true; }
    void push_back(std::unique_ptr<Job> job);

    template <typename T, typename... Args>
    void emplace_back(Args&&... args) {
        std::lock_guard<std::mutex> lock(m);
        if (!isOpen) {
            return;
        }
        q.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
        c.notify_one();
    }
    std::unique_ptr<Job> pop();
    void close();
    void open();
    void cancel();
    bool empty();
    size_t size();
};

}  // namespace ddb_ows

#endif

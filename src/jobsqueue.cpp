#include "jobsqueue.hpp"

#include <memory>

#include "job.hpp"

namespace ddb_ows {

void JobsQueue::push(std::unique_ptr<Job> job) {
    if (!isOpen) {
        return;
    }
    std::lock_guard<std::mutex> lock(m);
    q.push_back(std::move(job));
    c.notify_one();
}
std::unique_ptr<Job> JobsQueue::pop() {
    std::unique_lock<std::mutex> lock(m);
    c.wait(lock, [this] { return !this->q.empty() || !this->isOpen; });
    if (!this->q.empty()) {
        std::unique_ptr<Job> val = std::move(q.front());
        q.pop_front();
        return val;
    } else {
        return std::unique_ptr<Job>();
    }
}
void JobsQueue::close() {
    std::lock_guard<std::mutex> lock(m);
    isOpen = false;
    c.notify_all();
}

void JobsQueue::open() {
    std::lock_guard<std::mutex> lock(m);
    isOpen = true;
    c.notify_all();
}
void JobsQueue::cancel() {
    std::lock_guard<std::mutex> lock(m);
    isOpen = false;
    std::unique_ptr<Job> val;
    while (!q.empty()) {
        val = std::move(q.front());
        val->abort();
        q.pop_front();
    }
    c.notify_all();
}

bool JobsQueue::empty() {
    std::lock_guard<std::mutex> lock(m);
    bool out = q.empty();
    c.notify_all();
    return out;
}

int JobsQueue::size() {
    std::lock_guard<std::mutex> lock(m);
    int out = q.size();
    c.notify_all();
    return out;
}

}  // namespace ddb_ows

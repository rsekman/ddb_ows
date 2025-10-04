#ifndef DDB_OWS_PROGRESSMONITOR_HPP
#define DDB_OWS_PROGRESSMONITOR_HPP

#include <fmt/format.h>
#include <glibmm/dispatcher.h>
#include <gtkmm/progressbar.h>
#include <libnotify/notification.h>

#include <atomic>
#include <mutex>

class ProgressMonitor {
  public:
    ProgressMonitor(Gtk::ProgressBar* _pb);
    ~ProgressMonitor();
    ProgressMonitor(const ProgressMonitor& other) = delete;
    ProgressMonitor(ProgressMonitor&& other) = delete;

    void set_n_sources(size_t n);
    void job_queued();

    void set_n_jobs(size_t n);
    void job_finished();

    void free_notification();
    void cancel();

  private:
    void _job_queued();
    void _job_finished();

    void _cancel();

    size_t n_sources;
    std::atomic<size_t> n_queued = 0;
    size_t n_jobs;
    std::atomic<size_t> n_finished = 0;
    bool cancelled = false;

    Gtk::ProgressBar* pb;
    Glib::Dispatcher sig_job_queued, sig_job_finished, sig_cancel;

    std::mutex _m;
    NotifyNotification* notification = nullptr;
};

#endif

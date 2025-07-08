#ifndef DDB_OWS_PROGRESSMONITOR_HPP
#define DDB_OWS_PROGRESSMONITOR_HPP

#include <fmt/format.h>
#include <glibmm/dispatcher.h>
#include <gtkmm/progressbar.h>

class ProgressMonitor {
  public:
    ProgressMonitor(size_t (*r_jobs_)(), Gtk::ProgressBar* _pb);
    void set_n_jobs(size_t n);
    void tick();
    void pulse();
    void no_jobs();
    void cancel();

  private:
    void _tick();
    void _pulse();
    void _no_jobs();
    void _cancel();
    size_t n_jobs;
    bool cancelled = false;
    size_t (*r_jobs)();
    Gtk::ProgressBar* pb;
    Glib::Dispatcher sig_tick, sig_pulse, sig_no_jobs, sig_cancel;
};

#endif

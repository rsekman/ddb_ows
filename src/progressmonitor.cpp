#include "progressmonitor.hpp"
#include "log.hpp"

ProgressMonitor::ProgressMonitor(int (*r_jobs_)(), Gtk::ProgressBar* _pb) :
    r_jobs(r_jobs_), pb(_pb){
    sig_tick.connect(sigc::mem_fun(*this, &ProgressMonitor::_tick));
    sig_pulse.connect(sigc::mem_fun(*this, &ProgressMonitor::_pulse));
    sig_no_jobs.connect(sigc::mem_fun(*this, &ProgressMonitor::_no_jobs));
    sig_cancel.connect(sigc::mem_fun(*this, &ProgressMonitor::_cancel));
}

int ProgressMonitor::set_n_jobs(int n) {
    DDB_OWS_DEBUG << "ProgressMonitor set n_jobs = " << n << "..." << std::endl;
    return n_jobs = n;
}

void ProgressMonitor::cancel() {
    cancelled = true;
    sig_cancel();
}

void ProgressMonitor::tick() {
    sig_tick();
}

void ProgressMonitor::pulse() {
    sig_pulse();
}

void ProgressMonitor::no_jobs() {
    sig_no_jobs();
}

void ProgressMonitor::_tick() {
    if (pb == NULL || cancelled) {
        return;
    }
    int r = r_jobs();
    float pct = ((float) n_jobs - (float) r) / (float) n_jobs;
    pb->set_fraction(pct);
    pb->set_text(
        fmt::format("{}/{} ({:.0f}%)", n_jobs - r, n_jobs, 100*pct)
    );
    pb->queue_draw();
}

void ProgressMonitor::_pulse() {
    if (pb == NULL) {
        return;
    }
    pb->pulse();
    pb->queue_draw();
}

void ProgressMonitor::_no_jobs() {
    if (pb == NULL) {
        return;
    }
    pb->set_text("Nothing to do.");
    pb->set_fraction(1.0);
}

void ProgressMonitor::_cancel() {
    if (pb == NULL) {
        return;
    }
    pb->set_fraction(0.0);
    pb->set_text("Cancelled");
    pb->queue_draw();
}

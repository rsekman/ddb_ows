#include "gui/progressmonitor.hpp"

ProgressMonitor::ProgressMonitor(Gtk::ProgressBar* _pb) : pb(_pb) {
    sig_tick.connect(sigc::mem_fun(*this, &ProgressMonitor::_tick));
    sig_pulse.connect(sigc::mem_fun(*this, &ProgressMonitor::_pulse));
    sig_no_jobs.connect(sigc::mem_fun(*this, &ProgressMonitor::_no_jobs));
    sig_cancel.connect(sigc::mem_fun(*this, &ProgressMonitor::_cancel));
}

void ProgressMonitor::set_n_jobs(size_t n) {
    cancelled = false;
    n_jobs = n;
    n_finished = 0;
    if (pb == NULL) {
        return;
    }
    pb->set_fraction(0.0);
    pb->set_text(fmt::format("0/{} (0%)", n_jobs));
    pb->queue_draw();
}

void ProgressMonitor::cancel() {
    cancelled = true;
    sig_cancel();
}

void ProgressMonitor::tick() { sig_tick(); }

void ProgressMonitor::pulse() { sig_pulse(); }

void ProgressMonitor::no_jobs() { sig_no_jobs(); }

void ProgressMonitor::_tick() {
    if (pb == NULL || cancelled) {
        return;
    }
    // this method is only ever run in the Gtk main thread, so we don't need to
    // worry about concurrency
    n_finished += 1;
    float pct;
    if (n_jobs) {
        pct = (float)n_finished / (float)n_jobs;
    } else {
        pct = 1.0;
    }
    pb->set_fraction(pct);
    pb->set_text(fmt::format("{}/{} ({:.0f}%)", n_finished, n_jobs, 100 * pct));
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

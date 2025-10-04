#include "gui/progressmonitor.hpp"

#include <libnotify/notify.h>

void close_callback(NotifyNotification*, void* user_data) {
    auto ptr = static_cast<ProgressMonitor*>(user_data);
    ptr->free_notification();
};

ProgressMonitor::ProgressMonitor(Gtk::ProgressBar* _pb) : pb(_pb) {
    sig_job_queued.connect(sigc::mem_fun(*this, &ProgressMonitor::_job_queued));
    sig_job_finished.connect(
        sigc::mem_fun(*this, &ProgressMonitor::_job_finished)
    );

    sig_cancel.connect(sigc::mem_fun(*this, &ProgressMonitor::_cancel));

    if (notify_init("ddb_ows")) {
        notification = notify_notification_new("Started sync", "", nullptr);

        // When this is destroyed, it unrefs the notification, which finalizes
        // it, thus unregistering callbacks. That is, this always outlives
        // notification, so it is safe to pass it as a raw pointer.
        g_signal_connect(
            notification, "closed", (GCallback)close_callback, this

        );

        notify_notification_show(notification, nullptr);
    }
}

ProgressMonitor::~ProgressMonitor() { free_notification(); }

inline float pct(size_t n, size_t n_total) {
    float pct;
    if (n_total > 0) {
        pct = (float)n / (float)n_total;
    } else {
        pct = 1.0;
    }
    return pct;
}

void ProgressMonitor::set_n_sources(size_t n) {
    cancelled = false;
    n_sources = n;
    sig_job_queued();
}

std::pair<float, std::string> queue_progress(
    size_t n_queued, size_t n_sources
) {
    const float p = pct(n_queued, n_sources);
    return {
        p,
        fmt::format(
            "Queueing jobs ({}/{}, {:.0f}%)", n_queued, n_sources, 100 * p
        )
    };
}

void ProgressMonitor::job_queued() {
    n_queued += 1;
    sig_job_queued();
}

void ProgressMonitor::_job_queued() {
    // this method is only ever run in the Gtk main thread, so we don't need to
    // worry about concurrency

    if (cancelled) {
        return;
    }

    auto [p, text] = queue_progress(n_queued, n_sources);
    if (pb != nullptr) {
        pb->set_fraction(p);
        pb->set_text(text);
        pb->queue_draw();
    }

    std::lock_guard l{_m};
    if (notification != nullptr) {
        notify_notification_update(
            notification, "Syncing", text.c_str(), nullptr
        );
        notify_notification_show(notification, nullptr);
    }
}

void ProgressMonitor::set_n_jobs(size_t n) {
    cancelled = false;
    n_jobs = n;
    n_finished = 0;
    sig_job_finished();
}

std::pair<float, std::string> job_progress(size_t n_finished, size_t n_jobs) {
    if (n_jobs == 0) {
        return {1, "Nothing to do"};
    }

    const float p = pct(n_finished, n_jobs);
    return {
        p,
        fmt ::format(
            "Executing jobs ({}/{}, {:.0f}%)", n_finished, n_jobs, 100 * p
        )
    };
}

void ProgressMonitor::job_finished() {
    n_finished += 1;
    sig_job_finished();
}

void ProgressMonitor::_job_finished() {
    // this method is only ever run in the Gtk main thread, so we don't need to
    // worry about concurrency

    if (cancelled) {
        return;
    }

    auto [p, text] = job_progress(n_finished, n_jobs);
    if (pb != nullptr) {
        pb->set_fraction(p);
        pb->set_text(text);
        pb->queue_draw();
    }

    std::lock_guard l{_m};
    if (notification != nullptr) {
        notify_notification_update(
            notification, "Syncing", text.c_str(), nullptr
        );
        notify_notification_show(notification, nullptr);
    }
}

void ProgressMonitor::free_notification() {
    std::lock_guard l{_m};
    if (notification != nullptr) {
        g_object_unref(notification);
    }
    notification = nullptr;
}

void ProgressMonitor::cancel() {
    cancelled = true;
    sig_cancel();
}

void ProgressMonitor::_cancel() {
    if (pb != nullptr) {
        pb->set_fraction(0.0);
        pb->set_text("Cancelled");
        pb->queue_draw();
    }

    std::lock_guard l{_m};
    if (notification != nullptr) {
        notify_notification_update(notification, "Sync cancelled", "", nullptr);
        notify_notification_show(notification, nullptr);
    }
}

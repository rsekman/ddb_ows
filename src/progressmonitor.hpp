#ifndef DDB_OWS_PROGRESSMONITOR_HPP
#define DDB_OWS_PROGRESSMONITOR_HPP

#include <glibmm/dispatcher.h>
#include <gtkmm/progressbar.h>

#include <fmt/format.h>

#include "ddb_ows.hpp"

class ProgressMonitor {
    public:
        ProgressMonitor(ddb_ows_plugin_t* _ddb_ows, Gtk::ProgressBar* _pb);
        int set_n_jobs(int n);
        void tick();
        void pulse();
        void no_jobs();
        void cancel();
    private:
        void _tick();
        void _pulse();
        void _no_jobs();
        void _cancel();
        int n_jobs;
        bool cancelled = false;
        ddb_ows_plugin_t* ddb_ows;
        Gtk::ProgressBar* pb;
        Glib::Dispatcher sig_tick, sig_pulse, sig_no_jobs, sig_cancel;
};

#endif

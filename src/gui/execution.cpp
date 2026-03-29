#include <fmt/core.h>
#include <gtkmm/button.h>
#include <gtkmm/liststore.h>
#include <gtkmm/progressbar.h>

#include <thread>

#include "gui/internal.hpp"

namespace ddb_ows_gui {

using Button = Gtk::Button;

std::vector<ddb_playlist_t*> get_selected_playlists() {
    auto pl_model =
        Glib::RefPtr<Gtk::ListStore>::cast_static(plugin.builder->get_object("pl_selection_model"));
    auto pls = std::vector<ddb_playlist_t*>{};
    auto rows = pl_model->children();
    if (!std::size(rows)) {
        return pls;
    }
    for (auto r : rows) {
        bool pl_selected;
        ddb_playlist_t* pl_addr;
        r->get_value(0, pl_selected);
        if (pl_selected) {
            r->get_value(2, pl_addr);
            pls.push_back(pl_addr);
        }
    }
    return pls;
}

void execute(bool dry) {
    if (plugin.gui_logger) {
        plugin.gui_logger->clear();
    }

    Gtk::ProgressBar* pb;
    plugin.builder->get_widget("progress_bar", pb);
    if (pb) {
        plugin.pm = std::make_shared<ProgressMonitor>(pb);
    }

    playlist_save_cb_t pl_save_cb;
    if (pb != nullptr) {
        pl_save_cb = [pb](const char* ext) {
            pb->set_text(fmt::format("Saving playlists ({})", ext));
        };
    }

    sources_gathered_cb_t sources_gathered_cb;
    job_queued_cb_t job_queued_cb;
    job_finished_cb_t job_finished_cb;
    queueing_complete_cb_t q_complete_cb;
    if (plugin.pm != nullptr) {
        auto pm = plugin.pm;
        sources_gathered_cb = [pm](size_t n) { pm->set_n_sources(n); };
        job_queued_cb = [pm]() { pm->job_queued(); };
        q_complete_cb = [pm](size_t n) { pm->set_n_jobs(n); };
        job_finished_cb = [pm](std::unique_ptr<Job>, bool) { pm->job_finished(); };
    }

    callback_t callbacks{
        .on_playlist_save = pl_save_cb,
        .on_sources_gathered = sources_gathered_cb,
        .on_job_queued = job_queued_cb,
        .on_queueing_complete = q_complete_cb,
        .on_job_finished = job_finished_cb
    };

    std::vector<ddb_playlist_t*> pls = get_selected_playlists();

    std::shared_ptr<Logger> logger;
    if (plugin.gui_logger) {
        logger = plugin.gui_logger;
    } else {
        logger = std::make_shared<StdioLogger>();
    }
    std::thread t{[dry, pls, logger, callbacks] {
        ddb_ows->run(dry, pls, logger, callbacks);
        (*plugin.sig_execution_buttons_set_sensitive)();
    }};
    t.detach();
}

void execution_buttons_set_sensitive(bool sensitive) {
    Button* dry_run_btn = nullptr;
    Button* execute_btn = nullptr;
    plugin.builder->get_widget("execute_btn", execute_btn);
    plugin.builder->get_widget("dry_run_btn", dry_run_btn);
    if (dry_run_btn) {
        dry_run_btn->set_sensitive(sensitive);
    }
    if (execute_btn) {
        execute_btn->set_sensitive(sensitive);
    }
}

void execution_buttons_set_sensitive() { execution_buttons_set_sensitive(true); }
void execution_buttons_set_insensitive() { execution_buttons_set_sensitive(false); }

extern "C" {

void on_quit_btn_clicked() {
    GtkWidget* cwin = GTK_WIDGET(gtk_builder_get_object(plugin.builder->gobj(), "ddb_ows"));
    gtk_widget_destroy(cwin);
}

void on_cancel_btn_clicked(GtkButton* button, gpointer data) {
    plugin.pm->cancel();
    cancel_cb_t cb = []() { (*plugin.sig_execution_buttons_set_sensitive)(); };
    auto t = std::thread([cb]() { ddb_ows->cancel(cb); });
    t.detach();
}

void on_execution_btn_clicked(bool dry) {
    // signal handlers are called from the Gtk main thread, so we can set to
    // insensitive immediately
    execution_buttons_set_insensitive();
    execute(dry);
}

void on_dry_run_btn_clicked(GtkButton* button, gpointer data) { on_execution_btn_clicked(true); }

void on_execute_btn_clicked(GtkButton* button, gpointer data) { on_execution_btn_clicked(false); }

gboolean on_ddb_ows_key_press_event(GtkWidget* widget, GdkEventKey* key, gpointer data) {
    if (key->keyval == GDK_KEY_Escape) {
        gtk_widget_hide(widget);
        return TRUE;
    }
    return FALSE;
}

}  // extern "C"

}  // namespace ddb_ows_gui

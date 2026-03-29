#ifndef DDB_OWS_GUI_H
#define DDB_OWS_GUI_H

#include <deadbeef/deadbeef.h>
#include <gtk/gtk.h>
#include <gtkmm/builder.h>
#include <gtkmm/main.h>
#include <spdlog/logger.h>

#include "progressmonitor.hpp"
#include "textbufferlogger.hpp"

#if GTK_CHECK_VERSION(3, 0, 0)
#define DDB_OWS_GUI_PLUGIN_ID "ddb_ows_gtk3"
#define DDB_OWS_GUI_PLUGIN_NAME "ddb_ows_gtk3"
#else
#error "Only Gtk 3 is supported!"
#endif

namespace ddb_ows_gui {
// adapted from boost::hash_combine
inline size_t hash_combine(size_t x, size_t y) {
    x ^= y + 0x9e3779b97f4a7c16 + (x << 6) + (x >> 2);
    return x;
}

struct signal_handler_id {
    GObject* object;
    std::string signal_name;
    std::string handler_name;

    bool operator==(const signal_handler_id& other) const {
        return (object == other.object) && (signal_name == other.signal_name) &&
               (handler_name == other.handler_name);
    }
};

struct signal_handler_id_hash {
    std::size_t operator()(const signal_handler_id& id) const noexcept {
        const auto obj_hash = std::hash<const void*>{}(id.object);
        const auto sig_hash = std::hash<std::string>{}(id.signal_name);
        const auto handler_hash = std::hash<std::string>{}(id.handler_name);
        return hash_combine(hash_combine(obj_hash, sig_hash), handler_hash);
    }
};

using signal_map = std::unordered_map<signal_handler_id, gulong, signal_handler_id_hash>;

std::shared_ptr<spdlog::logger> get_logger();

struct ddb_ows_gui_plugin_t {
    DB_misc_t plugin;
    std::shared_ptr<Gtk::Main> app;
    std::shared_ptr<ProgressMonitor> pm;
    std::shared_ptr<ddb_ows::TextBufferLogger> gui_logger;
    std::shared_ptr<signal_map> signals;
    Glib::RefPtr<Gtk::Builder> builder;
    // These instances must be created by the Gtk main thread
    std::shared_ptr<Glib::Dispatcher> sig_execution_buttons_set_sensitive;
    std::shared_ptr<Glib::Dispatcher> sig_execution_buttons_set_insensitive;
};

}  // namespace ddb_ows_gui

#endif

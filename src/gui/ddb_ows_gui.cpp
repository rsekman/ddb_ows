#include "gui/ddb_ows_gui.hpp"

#include <deadbeef/gtkui_api.h>
#include <dlfcn.h>
#include <gtkmm/combobox.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/liststore.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/textview.h>
#include <gtkmm/window.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>

#include "ddb_ows.hpp"
#include "gui/converter.hpp"
#include "gui/execution.hpp"
#include "gui/fn_format.hpp"
#include "gui/misc.hpp"
#include "gui/playlist.hpp"

using namespace std::chrono_literals;

extern "C" {
DB_plugin_t* ddb_ows_gtk3_load(DB_functions_t* api);
}

namespace ddb_ows_gui {

DB_functions_t* ddb;

using namespace ddb_ows;

ddb_ows_gui_plugin_t plugin{
    .app = nullptr,
    .pm = nullptr,
    .gui_logger = nullptr,
    .signals = std::make_shared<signal_map>(),
    .builder = {},
    .sig_execution_buttons_set_sensitive = nullptr,
    .sig_execution_buttons_set_insensitive = nullptr,
};

ddb_ows_plugin_t* ddb_ows;

std::shared_ptr<spdlog::logger> get_logger() {
    auto logger = spdlog::get(DDB_OWS_GUI_PLUGIN_ID);
    return logger ? logger : spdlog::default_logger();
}

struct connect_args {
    GModule* gmodule;
    gpointer data;
    std::shared_ptr<signal_map> map;
};

static void gtk_builder_connect_signals_default(
    GtkBuilder* builder,
    GObject* object,
    const gchar* signal_name,
    const gchar* handler_name,
    GObject* connect_object,
    GConnectFlags flags,
    gpointer user_data
) {
    GCallback func;
    auto* args = static_cast<connect_args*>(user_data);

    if (!g_module_symbol(args->gmodule, handler_name, reinterpret_cast<gpointer*>(&func))) {
        g_warning("Could not find signal handler '%s'", handler_name);
        return;
    }

    gulong id;
    if (connect_object) {
        id = g_signal_connect_object(object, signal_name, func, connect_object, flags);
    } else {
        id = g_signal_connect_data(object, signal_name, func, args->data, nullptr, flags);
    }

    if (args->map != nullptr) {
        get_logger()->trace(
            "Connected {}::{} -> {} (id: {}). {} signals",
            static_cast<const void*>(object),
            signal_name,
            handler_name,
            id,
            args->map->size()
        );
        args->map->insert({{object, signal_name, handler_name}, id});
    }
}

int create_ui() {
    auto logger = get_logger();
    try {
        plugin.builder->add_from_resource("/ddb_ows/ddb_ows.ui");
    } catch (Gtk::BuilderError& e) {
        logger->critical("Could not build ui: {}.", std::string(e.what()));
        return -1;
    }
    auto provider = Gtk::CssProvider::create();
    provider->load_from_resource("/ddb_ows/ddb_ows.css");
    Gtk::StyleContext::add_provider_for_screen(
        Gdk::Screen::get_default(), provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    // Use introspection (dladdr) to figure out which file (.so) we are in.
    // This is necessary because we have to tell gtk to look for the signal
    // handlers in the .so, rather than in the main deadbeef executable.
    Dl_info dlinfo;
    int rc = dladdr(reinterpret_cast<void*>(&ddb_ows_gtk3_load), &dlinfo);
    if (!rc) {
        logger->critical("Could not determine .so location, quitting.");
        return -1;
    }

    logger->debug("Trying to load GModule {}", dlinfo.dli_fname);

    // Now we are ready to connect signal handlers;
    connect_args* args = g_slice_new0(connect_args);
    args->gmodule = g_module_open(dlinfo.dli_fname, G_MODULE_BIND_LAZY);
    args->data = nullptr;
    args->map = plugin.signals;
    gtk_builder_connect_signals_full(
        plugin.builder->gobj(), gtk_builder_connect_signals_default, args
    );

    plugin.sig_execution_buttons_set_sensitive = std::make_shared<Glib::Dispatcher>();
    plugin.sig_execution_buttons_set_sensitive->connect(
        sigc::ptr_fun<void>(execution_buttons_set_sensitive)
    );
    plugin.sig_execution_buttons_set_insensitive = std::make_shared<Glib::Dispatcher>();
    plugin.sig_execution_buttons_set_insensitive->connect(
        sigc::ptr_fun<void>(execution_buttons_set_insensitive)
    );

    auto pl_model =
        Glib::RefPtr<Gtk::ListStore>::cast_static(plugin.builder->get_object("pl_selection_model"));
    pl_selection_populate(pl_model, ddb_ows->conf->get_pl_selection());

    auto ft_model =
        Glib::RefPtr<Gtk::ListStore>::cast_static(plugin.builder->get_object("ft_model"));
    conv_fts_populate(ft_model);

    auto cp_model =
        Glib::RefPtr<Gtk::ListStore>::cast_static(plugin.builder->get_object("cp_model"));
    cp_populate(cp_model);

    Gtk::TextView* job_log;
    plugin.builder->get_widget("job_log", job_log);
    if (job_log) {
        auto log_buffer = Glib::RefPtr<Gtk::TextBuffer>::cast_static(
            plugin.builder->get_object("job_log_buffer")
        );
        plugin.gui_logger = std::make_shared<TextBufferLogger>(log_buffer, job_log);
        log_buffer->create_mark("END", log_buffer->end(), false);

        loglevel_cb_populate(plugin.gui_logger);
    }

    return 0;
}

int show_ui(DB_plugin_action_t* action, ddb_action_context_t ctx) {
    Gtk::Window* ddb_ows_win = nullptr;
    plugin.builder->get_widget("ddb_ows", ddb_ows_win);
    ddb_ows_win->present();
    ddb_ows_win->show_all();
    return 0;
}

static DB_plugin_action_t gui_action = {
    .title = "File/One-Way Sync",
    .name = "ddb_ows_ui",
    .flags = DB_ACTION_COMMON | DB_ACTION_ADD_MENU,
    .next = nullptr,
    .callback2 = show_ui,
};

DB_plugin_action_t* get_actions(DB_playItem_t* it) { return &gui_action; }

int start() { return 0; }

int stop() { return 0; }

int connect(void) {
    auto logger = get_logger();

    ddb_ows = reinterpret_cast<ddb_ows_plugin_t*>(ddb->plug_get_for_id("ddb_ows"));
    if (ddb_ows == nullptr) {
        logger->critical("ddb_ows plugin not found, quitting.");
        return -1;
    }

    DB_plugin_t* ddb_gtkui = ddb->plug_get_for_id(DDB_GTKUI_PLUGIN_ID);
    if (ddb_gtkui == nullptr) {
        logger->critical("Matching gtkui plugin not found, quitting.");
        return -1;
    }
    // Needed to make gtkmm play nice
    plugin.app = std::make_shared<Gtk::Main>();
    plugin.builder = Gtk::Builder::create();
    logger->info("Initialized successfully.");
    return create_ui();
}

int disconnect() {
    if (plugin.builder) {
        auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(
            plugin.builder->get_object("pl_selection_model")
        );
        pl_selection_clear(model);
    }
    plugin.pm.reset();
    plugin.gui_logger.reset();
    plugin.sig_execution_buttons_set_sensitive.reset();
    plugin.sig_execution_buttons_set_insensitive.reset();
    return 0;
}

int handleMessage(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2) {
    Glib::RefPtr<Gtk::ListStore> model;
    Gtk::ComboBox* fn_combobox;
    plugin.builder->get_widget("fn_format_combobox", fn_combobox);
    switch (id) {
        case DB_EV_PLAYLISTCHANGED:
            if (p1 == DDB_PLAYLIST_CHANGE_CONTENT || p1 == DDB_PLAYLIST_CHANGE_SELECTION ||
                p1 == DDB_PLAYLIST_CHANGE_SEARCHRESULT || p1 == DDB_PLAYLIST_CHANGE_PLAYQUEUE)
            {
                break;
            }
            model = Glib::RefPtr<Gtk::ListStore>::cast_static(
                plugin.builder->get_object("pl_selection_model")
            );
            pl_selection_update_model(model);
            break;
        case DB_EV_SONGCHANGED:
            if (fn_combobox->gobj()) {
                on_fn_format_combobox_changed(fn_combobox->gobj(), nullptr);
            }
            break;
    }
    return 0;
}

void init(DB_functions_t* api) {
    plugin.plugin = {
        .plugin = {
            .type = DB_PLUGIN_MISC,
            .api_vmajor = 1,
            .api_vminor = 8,
            .version_major = DDB_OWS_VERSION_MAJOR,
            .version_minor = DDB_OWS_VERSION_MINOR,
            .id = DDB_OWS_GUI_PLUGIN_ID,
            .name = DDB_OWS_GUI_PLUGIN_NAME,
            .descr = DDB_OWS_PROJECT_DESC,
            .copyright = DDB_OWS_LICENSE_TEXT,
            .website = DDB_OWS_PROJECT_URL,
            .start = start,
            .stop = stop,
            .connect = connect,
            .disconnect = disconnect,
            .get_actions = get_actions,
            .message = handleMessage,
            .configdialog = "",
        },
    };
}

}  // namespace ddb_ows_gui

DB_plugin_t* load(DB_functions_t* api) {
    ddb_ows_gui::ddb = api;
    ddb_ows_gui::init(api);
    auto logger = spdlog::stderr_color_mt(DDB_OWS_GUI_PLUGIN_ID);
    logger->set_level(spdlog::level::DDB_OWS_LOGLEVEL);
    logger->set_pattern("[%n] [%^%l%$] [thread %t] %v");
    return reinterpret_cast<DB_plugin_t*>(&ddb_ows_gui::plugin);
}

extern "C" DB_plugin_t* ddb_ows_gtk3_load(DB_functions_t* api) { return load(api); }

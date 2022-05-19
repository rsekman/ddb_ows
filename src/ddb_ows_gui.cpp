#include "gtkmm/togglebutton.h"
#include <gtkmm-2.4/gtkmm/filefilter.h>
#include <gtkmm.h>

#include <deadbeef/deadbeef.h>
#include <deadbeef/plugins/converter/converter.h>
#include <deadbeef/plugins/gtkui/gtkui_api.h>

#include <iostream>
#include <string>
#include <stdexcept>
#include <regex>

#include <filesystem>
namespace fs = std::filesystem;

#include <execinfo.h>

#include "ddb_ows.hpp"
#include "ddb_ows_gui.hpp"


DB_plugin_t definition_;
const char* configDialog_ = "";
static DB_functions_t* ddb_api;

Glib::RefPtr<Gtk::Builder> builder;

typedef struct {
    GModule* gmodule;
    gpointer data;
} connect_args;

static void
gtk_builder_connect_signals_default (GtkBuilder    *builder,
        GObject       *object,
        const gchar   *signal_name,
        const gchar   *handler_name,
        GObject       *connect_object,
        GConnectFlags  flags,
        gpointer       user_data)
{
    GCallback func;
    connect_args *args = (connect_args*)user_data;

    if (!g_module_symbol (args->gmodule, handler_name, (gpointer*)&func) )
    {
        g_warning ("Could not find signal handler '%s'", handler_name);
        return;
    }

    if (connect_object)
        g_signal_connect_object (object, signal_name, func, connect_object, flags);
    else
        g_signal_connect_data (object, signal_name, func, args->data, NULL, flags);
}

// TODO refactor this to take the model as an argument instead

auto get_pl_selection_model(){
    return Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("pl_selection_model")
    );
}

void pl_selection_check_consistency() {
    auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("pl_selection_model")
    );
    bool all_true  = true;
    bool all_false = true;
    bool pl_selected;
    auto rows = model->children();
    for(auto r = rows.begin(); r != rows.end(); r++) {
        r->get_value(0, pl_selected);
        all_true  = all_true && pl_selected;
        all_false = all_false && !pl_selected;
    }
    Gtk::ToggleButton* pl_select_all = NULL;
    builder->get_widget("pl_select_all", pl_select_all);
    DDB_OWS_DEBUG << "Playlist selection is " <<
        (all_true || all_false ? "consistent" : "inconsistent") << std::endl;
    pl_select_all->set_inconsistent(!(all_true || all_false));
}

bool validate_fn_format() {
    return true;
}

void update_fn_format_preview() {
}

void update_fn_format_model() {
}

/* BEGIN EXTERN SIGNAL HANDLERS */
// TODO consider moving these into their own file

extern "C"{

void on_pl_select_all_toggled(){
    DDB_OWS_DEBUG << "Toggling all selections." << std::endl;
    auto model = get_pl_selection_model();
    Gtk::ToggleButton* pl_select_all = NULL;
    builder->get_widget("pl_select_all", pl_select_all);
    bool sel =
        pl_select_all->get_inconsistent() ||
        !pl_select_all->get_active();
    model->foreach_iter(
        [sel, &model] ( const Gtk::TreeIter r) -> bool {
            r->set_value(0, sel);
            model->row_changed(Gtk::TreePath(r), r);
            return false;
        }
    );
    pl_select_all->set_active(sel);
    pl_selection_check_consistency();
}

void on_pl_selected_rend_toggled(GtkCellRendererToggle* rend, char* path, gpointer data){
    auto model = get_pl_selection_model();
    auto row = model->get_iter(path);
    bool pl_selected;
    row->get_value(0, pl_selected);
    row->set_value(0, !pl_selected);
    model->row_changed(Gtk::TreePath(path), row);
    pl_selection_check_consistency();
}

/* UI initalisation -- populate the various ListStores with data from DeadBeeF */

void populate_playlists() {
}

void populate_fn_formats() {
}

void populate_fts() {
}

void populate_presets() {
}

void init_target_root_directory() {
}

/* Handle config updates */

void on_target_root_chooser_file_set() {
}

void on_fn_format_combobox_changed() {
    if (!validate_fn_format()) {
        return;
    }
    update_fn_format_preview();
    update_fn_format_model();
}

/* Button actions */

void on_quit_btn_clicked(){
    GtkWidget* cwin = GTK_WIDGET( gtk_builder_get_object(builder->gobj(), "ddb_ows") );
    gtk_widget_destroy(cwin);
}

void on_cancel_btn_clicked(){
}

void on_dry_run_btn_clicked(){
}

void on_execute_btn_clicked(){
}

}

/* END EXTERN SIGNAL HANDLERS */

/* Build the UI from .ui */

int read_ui() {
    std::vector<std::string> plugdirs = {
        std::string( ddb_api->get_system_dir(DDB_SYS_DIR_PLUGIN)),
        std::string( LIBDIR ) + "/deadbeef"
    };
    std::string ui_fname;
    bool success = false;
    for( auto dir : plugdirs){
        ui_fname = dir + "/" + DDB_OWS_GUI_GLADE;
        try {
            DDB_OWS_DEBUG
                << "loading ui file "
                << ui_fname << std::endl;
            success = builder->add_from_file(ui_fname);
        } catch (Gtk::BuilderError& e) {
            DDB_OWS_ERR
                << "could not build ui from file "
                << ui_fname << ": "
                << e.what()
                << std::endl;
            continue;
        } catch (Glib::Error& e) {
            DDB_OWS_ERR
                << "could not load ui file "
                << ui_fname << ": "
                << e.what()
                << std::endl;
            continue;
        }
        if (success) {
            DDB_OWS_DEBUG
                << "loaded ui file "
                << ui_fname << std::endl;
            return 0;
        }
    }
    return -1;
}

int create_gui() {
    if(read_ui() < 0) {
        DDB_OWS_ERR << "Could not read .ui" << std::endl;
        return -1;
    }
    // Use introspection (backtrace) to figure out which file (.so) we are in.
    // This is necessary because we have to tell gtk to look for the signal
    // handlers in the .so, rather than in the main deadbeef executable.
    char** bt_symbols = NULL;
    void* trace[1];
    int trace_l = backtrace(trace, 1);
    bt_symbols = backtrace_symbols(trace, trace_l);
    // Backtrace looks something like "/usr/lib/deadbeef/ddb_ows_gtk2.so(create_gui+0x53) [0x7f7b0ae932a3]
    // We need to account for the possibility that the path is silly and contains '(' => use regex
    std::cmatch m;
    std::regex e("^(.*)\\(");
    std::regex_search(bt_symbols[0], m, e);
    DDB_OWS_DEBUG << "Trying to load GModule" << m[1] << std::endl;

    // Now we are ready to connect signal handlers;
    connect_args* args = g_slice_new0 (connect_args);
    args->gmodule = g_module_open (m[1].str().c_str(), G_MODULE_BIND_LAZY);
    args->data = NULL;
    gtk_builder_connect_signals_full(builder->gobj(),
        gtk_builder_connect_signals_default,
        args);

    pl_selection_check_consistency();
    return 0;
}

int show_gui(DB_plugin_action_t* action, ddb_action_context_t ctx) {
    Gtk::Window* ddb_ows_win = NULL;
    builder->get_widget("ddb_ows", ddb_ows_win);
    ddb_ows_win->present();
    return 0;
}

static DB_plugin_action_t gui_action = {
    .title = "File/One-Way Sync",
    .name = "ddb_ows_gui",
    .flags = DB_ACTION_COMMON | DB_ACTION_ADD_MENU,
    .next = NULL,
    .callback2 = show_gui,
};


DB_plugin_action_t * get_actions(DB_playItem_t *it) {
    return &gui_action;
}

int start() {
    return 0;
}

int stop() {
    return 0;
}

int connect (void) {
    gtkui_plugin = ddb_api->plug_get_for_id (DDB_GTKUI_PLUGIN_ID);
    converter_plugin = ddb_api->plug_get_for_id ("converter");
    ddb_ows_plugin = ddb_api->plug_get_for_id ("ddb_ows");
    if(!gtkui_plugin) {
        DDB_OWS_ERR << DDB_OWS_GUI_PLUGIN_NAME
            << ": matching gtkui plugin not found, quitting."
            << std::endl;
        return -1;
    }
    if(!converter_plugin) {
        fprintf(stderr, "%s: converter plugin not found\n", DDB_OWS_GUI_PLUGIN_NAME);
        return -1;
    }
    if(!ddb_ows_plugin) {
        fprintf(stderr, "%s: ddb_ows plugin not found\n", DDB_OWS_GUI_PLUGIN_NAME);
        return -1;
    }
    // Needed to make gtkmm play nice
    auto __attribute__((unused)) app = new Gtk::Main(0, NULL, false);
    builder = Gtk::Builder::create();
    return create_gui();
}

int disconnect(){
    return 0;
}

int handleMessage(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2){
    return 0;
}

void init(DB_functions_t* api) {
    definition_.api_vmajor = 1;
    definition_.api_vminor = 8;
    definition_.version_major = DDB_OWS_VERSION_MAJOR;
    definition_.version_minor = DDB_OWS_VERSION_MINOR;
    definition_.type = DB_PLUGIN_MISC;
    definition_.id = DDB_OWS_GUI_PLUGIN_ID;
    definition_.name = DDB_OWS_GUI_PLUGIN_NAME;
    definition_.descr = DDB_OWS_PROJECT_DESC;
    definition_.copyright = DDB_OWS_LICENSE_TEXT;
    definition_.website = DDB_OWS_PROJECT_URL;
    definition_.start = start;
    definition_.stop = stop;
    definition_.connect = connect;
    definition_.disconnect = disconnect;
    definition_.message = handleMessage;
    definition_.get_actions = get_actions;
    definition_.configdialog = configDialog_;
}

DB_plugin_t* load(DB_functions_t* api) {
    ddb_api = api;
    init(api);
    return &definition_;
}


extern "C" DB_plugin_t*
#if GTK_CHECK_VERSION(3,0,0)
ddb_ows_gtk3_load(DB_functions_t* api) {
#else
ddb_ows_gtk2_load(DB_functions_t* api) {
#endif
    return load(api);
}

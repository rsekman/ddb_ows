#include "gtkmm/builder.h"
#include "gtkmm/main.h"
#include "gtkmm/liststore.h"
#include "gtkmm/togglebutton.h"
#include "gtkmm/treeview.h"
#include "gtkmm/window.h"

#include <execinfo.h>

#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <regex>

namespace fs = std::filesystem;

#include <deadbeef/deadbeef.h>
#include <deadbeef/plugins/converter/converter.h>
#include <deadbeef/plugins/gtkui/gtkui_api.h>

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

void pl_selection_check_consistent(Glib::RefPtr<Gtk::ListStore> model) {
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
    pl_select_all->set_inconsistent(!(all_true || all_false));
}

bool validate_fn_format() {
    return true;
}

void update_fn_format_preview() {
}

void update_fn_format_model() {
}

void pl_selection_clear(Glib::RefPtr<Gtk::ListStore> model) {
    model->foreach_iter(
        [] ( const Gtk::TreeIter r) -> bool {
            ddb_playlist_t* plt;
            r->get_value(2, plt);
            ddb_api->plt_unref(plt);
            return false;
        }
    );
    model->clear();
}

void pl_selection_populate(
    Glib::RefPtr<Gtk::ListStore> model,
    std::map<ddb_playlist_t*, bool> selected={}
) {
    int plt_count = ddb_api->plt_get_count();
    char buf[4096];
    ddb_playlist_t*  plt;
    Gtk::TreeModel::iterator row;
    bool s;
    for(int i=0; i < plt_count; i++) {
        plt = ddb_api->plt_get_for_idx(i);
        ddb_api->plt_get_title(plt, buf, sizeof(buf));
        row = model->append();
        s = selected.count(plt) ? selected[plt] : false;
        row->set_value(0, s);
        row->set_value(1, std::string(buf));
        row->set_value(2, plt);
    }
}

void pl_selection_update_model(Glib::RefPtr<Gtk::ListStore> model) {
    // store each playlist's selection status in a map
    std::map<ddb_playlist_t*, bool> selected = {};
    model->foreach_iter(
        [&selected] ( const Gtk::TreeIter r) -> bool {
            bool s;
            r->get_value(0, s);
            ddb_playlist_t* p;
            r->get_value(2, p);
            selected[p] = s;
            return false;
        }
    );
    // now rebuild the model using the map to assign selection statuses
    pl_selection_clear(model);
    pl_selection_populate(model, selected);
}

void ft_populate(
    Glib::RefPtr<Gtk::ListStore> model,
    std::map<std::string, bool> selected={}
) {
    std::set<std::string> fts {};
    DB_decoder_t **decoders = ddb_api->plug_get_decoder_list ();
    // decoders and decoders[i]->exts are null-terminated arrays
    int i = 0;
    while (decoders[i]) {
        int e = 0;
        while(decoders[i]->exts[e]) {
            fts.insert(decoders[i]->exts[e]);
            e++;
        }
        i++;
    }
    Gtk::TreeModel::iterator row;
    DDB_OWS_DEBUG << "Read all filetypes..." << std::endl;
    for(auto ft = fts.begin(); ft != fts.end(); ft++) {
        row = model->append();
        row->set_value(0, false);
        row->set_value(1, *ft);
    }
}

/* BEGIN EXTERN SIGNAL HANDLERS */
// TODO consider moving these into their own file

extern "C"{

// TODO
// This method and the following are actually agnostic re: which model we are
// selecting/unselecting in. We should refactor them so we can reuse them for
// the ft selection model

void on_pl_select_all_toggled(GtkListStore* ls, gpointer data){
    // Taking ownership of the instance can lead to incorrect reference counts
    // so we must pass true as the second argument to take a new copy or ref
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    Gtk::ToggleButton* pl_select_all = NULL;
    builder->get_widget("pl_select_all", pl_select_all);
    bool sel =
        pl_select_all->get_inconsistent() ||
        !pl_select_all->get_active();
    model->foreach_iter(
        [sel, &model] ( const Gtk::TreeIter r) -> bool {
            r->set_value(0, sel);
            std::string n;
            r->get_value(1, n);
            model->row_changed(Gtk::TreePath(r), r);
            return false;
        }
    );
    pl_select_all->set_active(sel);
    pl_selection_check_consistent(model);
}

void on_pl_selected_rend_toggled(GtkCellRendererToggle* rend, char* path, gpointer data){
    if (data == NULL) {
        return;
    }
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(GTK_LIST_STORE (data), true);
    auto row = model->get_iter(path);
    bool pl_selected;
    row->get_value(0, pl_selected);
    row->set_value(0, !pl_selected);
    model->row_changed(Gtk::TreePath(path), row);
    pl_selection_check_consistent(model);
}

/* UI initalisation -- populate the various ListStores with data from DeadBeeF */

void pl_selection_populate(GtkListStore* ls, gpointer data) {
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    pl_selection_populate(model);
}

void pl_selection_clear(GtkListStore* ls, gpointer data) {
    auto model_ptr = Glib::wrap(ls, true);
    auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(model_ptr);
    pl_selection_clear(model);
}

void populate_fn_formats() {
}

void ft_populate(GtkListStore* ls, gpointer data) {
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    ft_populate(model);
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

int create_ui() {
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
    // Backtrace looks something like "/usr/lib/deadbeef/ddb_ows_gtk2.so(create_ui+0x53) [0x7f7b0ae932a3]
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

    auto pl_model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("pl_selection_model")
    );
    pl_selection_clear(pl_model);
    pl_selection_check_consistent(pl_model);

    auto ft_model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("ft_model")
    );
    ft_populate(ft_model);

    return 0;
}

int show_ui(DB_plugin_action_t* action, ddb_action_context_t ctx) {
    Gtk::Window* ddb_ows_win = NULL;
    builder->get_widget("ddb_ows", ddb_ows_win);
    ddb_ows_win->present();
    return 0;
}

static DB_plugin_action_t gui_action = {
    .title = "File/One-Way Sync",
    .name = "ddb_ows_ui",
    .flags = DB_ACTION_COMMON | DB_ACTION_ADD_MENU,
    .next = NULL,
    .callback2 = show_ui,
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
    return create_ui();
}

int disconnect(){
    auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("pl_selection_model")
    );
    pl_selection_clear(model);
    return 0;
}

int handleMessage(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2){
    switch (id) {
        case DB_EV_PLAYLISTCHANGED:
            auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(
                builder->get_object("pl_selection_model")
            );
            pl_selection_update_model(model);
            break;
    }
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

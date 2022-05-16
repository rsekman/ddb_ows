#include <gtkmm.h>

#include <deadbeef/deadbeef.h>
#include <deadbeef/plugins/converter/converter.h>
#include <deadbeef/plugins/gtkui/gtkui_api.h>

#include <iostream>
#include <string>
#include <stdexcept>

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

extern "C"{
void on_target_browser_selection_changed(GtkFileChooser* target_browser, gpointer data) {
}

void on_playlist_select_all_toggled(){
}

void on_pl_selected_rend_toggled(){
}

void on_target_dir_entry_changed(){
}

void on_quit_btn_clicked(){
    GtkWidget* cwin = GTK_WIDGET( gtk_builder_get_object(builder->gobj(), "ddb_ows") );
    gtk_widget_destroy(cwin);
}
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
    auto app = new Gtk::Main(0, NULL, false);
    builder = Gtk::Builder::create();
    return 0;
}

int disconnect(){
    return 0;
}

int handleMessage(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2){
    return 0;
}

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
                << ui_fname << ": "
                << std::endl;
            return 0;
        }
    }
    return -1;
}

int create_gui(DB_plugin_action_t* action, ddb_action_context_t ctx) {
    if(read_ui() < 0) {
        DDB_OWS_ERR << "Could not read .ui" << std::endl;
        return -1;
    }
    DDB_OWS_DEBUG << "Trying to load GModule" << DDB_OWS_LIB_FILE << std::endl;
    connect_args* args;
    args = g_slice_new0 (connect_args);
    args->gmodule = g_module_open (DDB_OWS_LIB_FILE, G_MODULE_BIND_LAZY);
    args->data = NULL;
    gtk_builder_connect_signals_full(builder->gobj(),
        gtk_builder_connect_signals_default,
        args);

    //GObject* quit_btn = gtk_builder_get_object(gbuilder, "quit_btn");
    //g_signal_connect(quit_btn, "clicked", G_CALLBACK(on_quit_btn_clicked), NULL);

    Gtk::Button* quit_btn = NULL;
    builder->get_widget("quit_btn", quit_btn);

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
    .callback2 = create_gui,
};


DB_plugin_action_t * get_actions(DB_playItem_t *it) {
    return &gui_action;
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

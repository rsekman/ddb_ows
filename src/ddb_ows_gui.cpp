#include "glibmm/refptr.h"
#include "gtkmm/builder.h"
#include "gtkmm/checkbutton.h"
#include "gtkmm/combobox.h"
#include "gtkmm/filechooserbutton.h"
#include "gtkmm/liststore.h"
#include "gtkmm/main.h"
#include "gtkmm/progressbar.h"
#include "gtkmm/stock.h"
#include "gtkmm/togglebutton.h"
#include "gtkmm/spinbutton.h"
#include "gtkmm/treeview.h"
#include "gtkmm/window.h"

#include <execinfo.h>

#include <filesystem>
#include <functional>
#include <gtkmm-2.4/gtkmm/textbuffer.h>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>


#include <deadbeef/deadbeef.h>
#include <deadbeef/plugins/converter/converter.h>
#include <deadbeef/plugins/gtkui/gtkui_api.h>

#include "ddb_ows.hpp"
#include "ddb_ows_gui.hpp"

#include "textbufferlogger.hpp"

DB_plugin_t definition_;
const char* configDialog_ = "";
static DB_functions_t* ddb;

namespace ddb_ows_gui {

using namespace ddb_ows;

ddb_ows_plugin_t* ddb_ows;

std::optional<TextBufferLogger> gui_logger {};
StdioLogger terminal_logger {};

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

void list_store_check_consistent(
    Glib::RefPtr<Gtk::ListStore> model,
    Gtk::CheckButton* toggle,
    int col = 0
) {
    // Check if the bool values in column col of model are all true or all false
    // Update the toggle's inconsistent state accordingly
    bool all_true  = true;
    bool all_false = true;
    bool pl_selected;
    if(!model) {
        DDB_OWS_WARN << "Attempt to check consistency with null model." << std::endl;
        return;
    }
    if(!toggle) {
        DDB_OWS_WARN << "Attempt to check consistency with null toggle " << toggle << std::endl;
        return;
    }
    auto rows = model->children();
    if(!std::size(rows)){
        return;
    }
    for(auto r = rows.begin(); r != rows.end(); r++) {
        r->get_value(0, pl_selected);
        all_true  = all_true && pl_selected;
        all_false = all_false && !pl_selected;
    }
    toggle->set_inconsistent(!(all_true || all_false));
    toggle->set_active(all_true);
}

void fn_formats_save(Glib::RefPtr<Gtk::ListStore> model){
    std::vector<std::string> fmts {};
    model->foreach_iter(
        [&fmts] (const Gtk::TreeIter r) -> bool {
            std::string f;
            r->get_value(0, f);
            fmts.push_back(f);
            return false;
        }
    );
    ddb_ows->conf.set_fn_formats(fmts);
}

void fn_formats_populate(Glib::RefPtr<Gtk::ListStore> model) {
    std::vector<std::string> fmts = ddb_ows->conf.get_fn_formats();
    if (!fmts.size()) {
        // No formats save in config => use formats from .ui file
        // This has the side effect of bootstrapping the user's config from the .ui file
        return;
    }
    model->clear();
    Gtk::TreeModel::iterator r;
    for(auto i = fmts.begin(); i != fmts.end(); i++) {
        DDB_OWS_DEBUG << "Appending " << *i << " to fn formats" << std::endl;
        r = model->append();
        r->set_value(0, *i);
    }
}

char* validate_fn_format(std::string fmt) {
    char* bc = ddb->tf_compile(fmt.c_str());
    Gtk::Image* valid_indicator;
    builder->get_widget("fn_format_valid_indicator", valid_indicator);
    Gtk::BuiltinStockID icon;
    if (bc != NULL) {
        icon = Gtk::Stock::YES;
    } else {
        icon = Gtk::Stock::NO;
    }
    valid_indicator->set(icon, Gtk::ICON_SIZE_MENU);
    return bc;
}

void clear_fn_preview() {
    Gtk::Label* preview_label;
    builder->get_widget("fn_format_preview_label", preview_label);
    preview_label->set_text("");
}

void update_fn_preview(char* format) {
    if (format == NULL) {
        // This should never happen
        return;
    }
    Gtk::Label* preview_label;
    builder->get_widget("fn_format_preview_label", preview_label);
    DB_playItem_t* it = ddb->streamer_get_playing_track();
    if (!it) {
        //Pick a random track from the current playlist
        ddb->pl_lock();
        ddb_playlist_t* plt = ddb->plt_get_curr();
        if (!plt) {
            ddb->pl_unlock();
            return;
        }
        it = ddb->plt_get_first(plt, PL_MAIN);
        ddb->plt_unref(plt);
        ddb->pl_unlock();
    }
    std::string out = ddb_ows->get_output_path(it, format);
    ddb->tf_free(format);
    ddb->pl_item_unref(it);
    preview_label->set_text(out);
    preview_label->queue_resize();
}

void cp_populate(Glib::RefPtr<Gtk::ListStore> model) {
    ddb_converter_t* enc_plug = (ddb_converter_t*) ddb->plug_get_for_id("converter");
    if (enc_plug == NULL) {
        DDB_OWS_WARN << "Converter plugin not present!" << std::endl;
        return;
    }
    ddb_encoder_preset_t* enc = enc_plug->encoder_preset_get_list();
    Gtk::TreeModel::iterator row;
    Gtk::ComboBox* cp_combobox;
    builder->get_widget("cp_combobox", cp_combobox);
    std::string conf_preset_name = ddb_ows->conf.get_conv_preset();
    while ( enc != NULL ) {
        row = model->append();
        std::string preset_name = std::string(enc->title);
        row->set_value(0, preset_name);
        row->set_value(1, enc);
        if (conf_preset_name == preset_name) {
            cp_combobox->set_active(row);
        }
        enc = enc->next;
    }
}


void pl_selection_clear(Glib::RefPtr<Gtk::ListStore> model) {
    DDB_OWS_DEBUG << "Clearing playlist selection model." << std::endl;
    model->foreach_iter(
        [] ( const Gtk::TreeIter r) -> bool {
            ddb_playlist_t* plt;
            r->get_value(2, plt);
            ddb->plt_unref(plt);
            return false;
        }
    );
    Gtk::CheckButton* toggle;
    builder->get_widget("pl_select_all", toggle);
    model->clear();
}

void pl_selection_populate(
    Glib::RefPtr<Gtk::ListStore> model,
    std::unordered_map<ddb_playlist_t*, bool> selected={}
) {
    int plt_count = ddb->plt_get_count();
    DDB_OWS_DEBUG << "Populating playlist selection model with " << plt_count << " playlists." << std::endl;
    char buf[4096];
    ddb_playlist_t*  plt;
    Gtk::TreeModel::iterator row;
    bool s;
    for(int i=0; i < plt_count; i++) {
        plt = ddb->plt_get_for_idx(i);
        ddb->plt_get_title(plt, buf, sizeof(buf));
        row = model->append();
        s = selected.count(plt) ? selected[plt] : false;
        row->set_value(0, s);
        row->set_value(1, std::string(buf));
        row->set_value(2, plt);
    }
}

void pl_selection_update_model(Glib::RefPtr<Gtk::ListStore> model) {
    // store each playlist's selection status in a map
    std::unordered_map<ddb_playlist_t*, bool> selected = {};
    ddb->pl_lock();
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
    Gtk::CheckButton* toggle;
    builder->get_widget("pl_select_all", toggle);
    pl_selection_clear(model);
    pl_selection_populate(model, selected);
    ddb->pl_unlock();
}

void queue_jobs() {
    auto pl_model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("pl_selection_model")
    );
    auto pls = std::vector<ddb_playlist_t*> {};
    auto rows = pl_model->children();
    if(!std::size(rows)){
        return;
    }
    for(auto r = rows.begin(); r != rows.end(); r++) {
        bool pl_selected;
        ddb_playlist_t* pl_addr;
        r->get_value(0, pl_selected);
        if (pl_selected) {
            r->get_value(2, pl_addr);
            pls.push_back(pl_addr);
        }
    }
    if (gui_logger) {
        ddb_ows->queue_jobs(pls, gui_logger.value());
    } else {
        ddb_ows->queue_jobs(pls, terminal_logger);
    }
}

void conv_fts_save(Glib::RefPtr<Gtk::ListStore> model){
    std::unordered_map<std::string, bool> fts {};
    model->foreach_iter(
        [&fts] (const Gtk::TreeIter r) -> bool {
            std::string name;
            bool checked;
            r->get_value(0, checked);
            r->get_value(1, name);
            if(checked) {
                fts[name] = true;
            }
            return false;
        }
    );
    ddb_ows->conf.set_conv_fts(fts);
}

void conv_fts_populate(
    Glib::RefPtr<Gtk::ListStore> model,
    std::unordered_map<std::string, bool> selected={}
) {
    DB_decoder_t **decoders = ddb->plug_get_decoder_list ();
    // decoders and decoders[i]->exts are null-terminated arrays
    int i = 0;
    std::string::size_type n;
    Gtk::TreeModel::iterator row;
    std::unordered_map<std::string, bool> sels = ddb_ows->conf.get_conv_fts();
    while (decoders[i]) {
        row = model->append();
        std::string s(decoders[i]->plugin.name);
        if (
            (n = s.find(" decoder")) != std::string::npos ||
            (n = s.find(" player"))  != std::string::npos
        ) {
            s = s.substr(0, n);
        }
        row->set_value(0, sels.count(s) > 0);
        row->set_value(1, s);
        row->set_value(2, decoders[i]);
        i++;
    }
    DDB_OWS_DEBUG << "Finished reading decoders..." << std::endl;
}

/* BEGIN EXTERN SIGNAL HANDLERS */
// TODO consider moving these into their own file

job_cb_t make_progress_callback() {
    ddb_ows_plugin_t* ddb_ows = (ddb_ows_plugin_t*) ddb->plug_get_for_id("ddb_ows");
    int n_jobs = ddb_ows->jobs_count();
    Gtk::ProgressBar* pb;
    builder->get_widget("progress_bar", pb);
    job_cb_t callback;
    if (pb != NULL) {
        return [n_jobs, ddb_ows, pb](std::unique_ptr<Job>) {
            pb->set_fraction(
                ((float) n_jobs - (float)ddb_ows->jobs_count()) / (float)n_jobs
            );
            pb->queue_draw();
        };
    } else {
        return job_cb_t();
    }
}

void execute(bool dry) {
    queue_jobs();
    Gtk::ProgressBar* pb;
    builder->get_widget("progress_bar", pb);
    if (pb != NULL) {
        pb->set_fraction(0);
    }
    ddb_ows_plugin_t* ddb_ows = (ddb_ows_plugin_t*) ddb->plug_get_for_id("ddb_ows");
    std::thread( [ddb_ows, dry]{
        ddb_ows->run(dry, make_progress_callback());
    }).detach();
}

extern "C" {

// TODO
// This method and the following are actually agnostic re: which model we are
// selecting/unselecting in. We should refactor them so we can reuse them for
// the ft selection model

void on_select_all_toggled(GtkListStore* ls, gpointer data){
    // Taking ownership of the instance can lead to incorrect reference counts
    // so we must pass true as the second argument to take a new copy or ref
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    Gtk::CheckButton* toggle = Glib::wrap(
        GTK_CHECK_BUTTON (gtk_tree_view_column_get_widget( GTK_TREE_VIEW_COLUMN( data )) ),
        true);
    bool sel =
        toggle->get_inconsistent() ||
        !toggle->get_active();
    model->foreach_iter(
        [sel, &model] ( const Gtk::TreeIter r) -> bool {
            r->set_value(0, sel);
            std::string n;
            r->get_value(1, n);
            model->row_changed(Gtk::TreePath(r), r);
            return false;
        }
    );
    toggle->set_active(sel);
}

void on_selected_rend_toggled(GtkCellRendererToggle* rend, char* path, gpointer data){
    if (data == NULL) {
        return;
    }
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(GTK_LIST_STORE (data), true);
    auto row = model->get_iter(path);
    bool pl_selected;
    row->get_value(0, pl_selected);
    row->set_value(0, !pl_selected);
    model->row_changed(Gtk::TreePath(path), row);
}

void list_store_check_consistent(
    GtkListStore* ls,
    GtkTreePath *path,
    GtkTreeIter *iter,
    gpointer data
){
    if (data == NULL) {
        return;
    }
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    Gtk::CheckButton* toggle = Glib::wrap(GTK_CHECK_BUTTON( data ), true);
    list_store_check_consistent(model, toggle);
}

// needed because row-deleted has a different call signature than row-changed and row-inserted
void list_store_check_consistent_on_delete(
    GtkListStore* ls,
    GtkTreePath *path,
    gpointer data
){
    list_store_check_consistent(ls, path, NULL, data);
}

/* UI initalisation -- populate the various ListStores with data from DeadBeeF */

void cp_populate(GtkListStore* ls, gpointer data) {
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    cp_populate(model);
}

/* Handle config updates */

void on_target_root_chooser_selection_changed(GtkFileChooserButton* fcb, gpointer data) {
    // We connect to this signal because file-set is only emitted if the file
    // browser was opened, not if the target was chosen from the drop-down
    // menu.
    GFile* root = gtk_file_chooser_get_file( GTK_FILE_CHOOSER(fcb) );
    char* root_path = g_file_get_path(root);
    ddb_ows->conf.set_root( std::string(root_path) );
    g_free(root_path);
    g_object_unref(root);
}

void fn_formats_save(
    GtkListStore* ls,
    GtkTreePath *path,
    GtkTreeIter *iter,
    gpointer data
){
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    fn_formats_save(model);
}

// needed because row-deleted has a different call signature than row-changed and row-inserted
void fn_formats_save_on_delete(
    GtkListStore* ls,
    GtkTreePath *path,
    gpointer data
){
    fn_formats_save(ls, path, NULL, data);
}

void on_fn_format_entered(Gtk::Entry* entry) {
    auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("fn_format_model")
    );
    std::string fn_format = entry->get_text();
    char* format = validate_fn_format(fn_format);
    if (format == NULL) {
        return;
    }
    auto rows = model->children();
    std::string f;
    Gtk::ComboBox* fn_combobox;
    builder->get_widget("fn_format_combobox", fn_combobox);
    for(auto i = rows.begin(); i != rows.end(); i++) {
        i->get_value(0, f);
        if (f == fn_format) {
            model->move(i, rows.begin());
            fn_combobox->set_active(0);
            return;
        }
    }
    auto row = model->prepend();
    row->set_value(0, entry->get_text());
    update_fn_preview(format);
}

bool on_fn_format_focus_out(GdkEventFocus* event, Gtk::Entry* entry) {
    on_fn_format_entered(entry);
    return false;
}

void on_fn_format_combobox_changed(GtkComboBox* fn_combobox, gpointer data) {
    std::string fn_format;
    gint active = gtk_combo_box_get_active(fn_combobox);
    if (active >= 0) {
        auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(
            builder->get_object("fn_format_model")
        );
        auto rows = model->children();
        rows[active]->get_value(0, fn_format);
        model->move(rows[active], rows.begin());
    } else {
        GtkEntry* entry = GTK_ENTRY (gtk_bin_get_child( GTK_BIN (fn_combobox) ));
        fn_format = gtk_entry_get_text(entry);
    }

    char* format = validate_fn_format(fn_format);
    if (format != NULL) {
        update_fn_preview(format);
    } else {
        clear_fn_preview();
    }
}

/* Initialize the UI with values from config */

void on_cover_fname_entry_show(GtkWidget* widget, gpointer data) {
     gtk_entry_set_text(
         GTK_ENTRY(widget),
         ddb_ows->conf.get_cover_fname().c_str()
    );
}

void on_conv_ext_entry_show(GtkWidget* widget, gpointer data) {
    gtk_entry_set_text(
        GTK_ENTRY(widget),
        ddb_ows->conf.get_conv_ext().c_str()
   );
}

void on_target_root_chooser_show(GtkWidget* widget, gpointer data) {
    std::string root = ddb_ows->conf.get_root();
    DDB_OWS_DEBUG << "setting root to" << root << std::endl;
    const char* path = root.c_str();
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(widget), path);
}

void on_fn_format_combobox_show(GtkWidget* widget, gpointer data) {
    GtkListStore* model = GTK_LIST_STORE( gtk_combo_box_get_model( GTK_COMBO_BOX(widget)) );
    auto fn_model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        Glib::wrap(model)
    );
    fn_formats_populate(fn_model);
    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);

    Gtk::ComboBox* fn_combobox = Glib::wrap(GTK_COMBO_BOX(widget), true);
    Gtk::Entry* fn_entry = (Gtk::Entry*) fn_combobox->get_child();
    fn_entry->signal_activate().connect(
        sigc::bind(
            sigc::ptr_fun(&on_fn_format_entered),
            fn_entry
        )
    );
    fn_entry->signal_focus_out_event().connect(
        sigc::bind(
            sigc::ptr_fun(&on_fn_format_focus_out),
            fn_entry
        )
    );
}

void on_cover_sync_check_show(GtkWidget* widget, gpointer data) {
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(widget),
        ddb_ows->conf.get_cover_sync()
    );
}

void on_rm_unref_check_show(GtkWidget* widget, gpointer data) {
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(widget),
        ddb_ows->conf.get_rm_unref()
    );
}

void on_wt_spinbutton_show(GtkWidget* widget, gpointer data) {
    gtk_spin_button_set_value(
        GTK_SPIN_BUTTON(widget),
        ddb_ows->conf.get_conv_wts()
    );
}

/* Save values to config when changed in the UI */

void on_cover_fname_entry_changed(GtkEntry* entry, gpointer data) {
    const gchar* cover_fname = gtk_entry_get_text(entry);
    ddb_ows->conf.set_cover_fname(std::string(cover_fname));
}

void on_cover_sync_check_toggled(GtkToggleButton* toggle, gpointer data) {
    gboolean cover_sync = gtk_toggle_button_get_active(toggle);
    ddb_ows->conf.set_cover_sync(cover_sync);
}

void on_rm_check_toggled(GtkToggleButton* toggle, gpointer data) {
    gboolean rm_unref = gtk_toggle_button_get_active(toggle);
    ddb_ows->conf.set_rm_unref(rm_unref);
}

void on_wt_spinbutton_value_changed(GtkSpinButton* spinbutton, gpointer data) {
    int wt = (int) gtk_spin_button_get_value(spinbutton);
    DDB_OWS_DEBUG << "Saving wts " << wt << std::endl;
    ddb_ows->conf.set_conv_wts(wt);
}

void conv_fts_save(
    GtkListStore* ls,
    GtkTreePath *path,
    GtkTreeIter *iter,
    gpointer data
){
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    conv_fts_save(model);
}

void on_conv_ext_entry_changed(GtkEntry* entry, gpointer data) {
    const gchar* conv_ext = gtk_entry_get_text(entry);
    ddb_ows->conf.set_conv_ext(std::string(conv_ext));
}

void on_cp_combobox_changed(GtkComboBox* combobox, gpointer data) {
    DDB_OWS_DEBUG << "Preset changed" << std::endl;
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(GTK_LIST_STORE(data), true);
    Gtk::ComboBox* cb = Glib::wrap(combobox, true);
    auto iter = cb->get_active();
    std::string out;
    iter->get_value(0, out);
    ddb_ows->conf.set_conv_preset(out);
}

/* Clean-up actions */

void pl_selection_clear(GtkListStore* ls, gpointer data){
    auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        Glib::wrap(ls)
    );
    pl_selection_clear(model);
}

/* Button actions */

void on_quit_btn_clicked(){
    GtkWidget* cwin = GTK_WIDGET( gtk_builder_get_object(builder->gobj(), "ddb_ows") );
    gtk_widget_destroy(cwin);
}

void on_cancel_btn_clicked(){
}

void on_dry_run_btn_clicked(GtkButton* button, gpointer data){
    execute(true);
}

void on_execute_btn_clicked(GtkButton* button, gpointer data){
    execute(false);
}

}

/* END EXTERN SIGNAL HANDLERS */

/* Build the UI from .ui */

int read_ui() {
    std::vector<std::string> plugdirs = {
        std::string( ddb->get_system_dir(DDB_SYS_DIR_PLUGIN)),
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
    // We need to account for the possibility that the path is silly and contains '(' => find last position
    char* last_bracket = strrchr(bt_symbols[0], '(');
    if (last_bracket) {
        *last_bracket = '\0';
    }
    DDB_OWS_DEBUG << "Trying to load GModule" << bt_symbols[0] << std::endl;

    // Now we are ready to connect signal handlers;
    connect_args* args = g_slice_new0 (connect_args);
    args->gmodule = g_module_open (bt_symbols[0], G_MODULE_BIND_LAZY);
    args->data = NULL;
    gtk_builder_connect_signals_full(builder->gobj(),
        gtk_builder_connect_signals_default,
        args);

    auto pl_model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("pl_selection_model")
    );
    pl_selection_update_model(pl_model);

    auto ft_model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("ft_model")
    );
    conv_fts_populate(ft_model);

    auto cp_model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("cp_model")
    );
    cp_populate(cp_model);

    auto log_buffer = Glib::RefPtr<Gtk::TextBuffer>::cast_static(
        builder->get_object("job_log_buffer")
    );
    if (log_buffer) {
        gui_logger.emplace( log_buffer );
    }

    return 0;
}

int show_ui(DB_plugin_action_t* action, ddb_action_context_t ctx) {
    Gtk::Window* ddb_ows_win = NULL;
    builder->get_widget("ddb_ows", ddb_ows_win);
    ddb_ows_win->present();
    ddb_ows_win->show_all();
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
    DB_plugin_t* ddb_gtkui = ddb->plug_get_for_id (DDB_GTKUI_PLUGIN_ID);
    ddb_converter_t* ddb_converter = (ddb_converter_t*) ddb->plug_get_for_id ("converter");
    ddb_ows = (ddb_ows_plugin_t*) ddb->plug_get_for_id ("ddb_ows");
    if(!ddb_gtkui) {
        DDB_OWS_ERR << DDB_OWS_GUI_PLUGIN_NAME
            << ": matching gtkui plugin not found, quitting."
            << std::endl;
        return -1;
    }
    if(!ddb_converter) {
        fprintf(stderr, "%s: converter plugin not found\n", DDB_OWS_GUI_PLUGIN_NAME);
        return -1;
    }
    if(!ddb_ows) {
        fprintf(stderr, "%s: ddb_ows plugin not found\n", DDB_OWS_GUI_PLUGIN_NAME);
        return -1;
    }
    // Needed to make gtkmm play nice
    auto __attribute__((unused)) app = new Gtk::Main(0, NULL, false);
    builder = Gtk::Builder::create();
    return create_ui();
}

int disconnect(){
    if (builder) {
        auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(
            builder->get_object("pl_selection_model")
        );
        pl_selection_clear(model);
    }
    return 0;
}

int handleMessage(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2){
    Glib::RefPtr<Gtk::ListStore> model;
    Gtk::ComboBox* fn_combobox;
    builder->get_widget("fn_format_combobox", fn_combobox);
    switch (id) {
        case DB_EV_PLAYLISTCHANGED:
            if (p1 == DDB_PLAYLIST_CHANGE_CONTENT ||
                p1 == DDB_PLAYLIST_CHANGE_SELECTION ||
                p1 == DDB_PLAYLIST_CHANGE_SEARCHRESULT ||
                p1 == DDB_PLAYLIST_CHANGE_PLAYQUEUE
            ) {
                break;
            }
            model = Glib::RefPtr<Gtk::ListStore>::cast_static(
                builder->get_object("pl_selection_model")
            );
            pl_selection_update_model(model);
            break;
        case DB_EV_SONGCHANGED:
            if(fn_combobox->gobj()) {
                on_fn_format_combobox_changed(fn_combobox->gobj(), NULL);
            }
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

}

DB_plugin_t* load(DB_functions_t* api) {
    ddb = api;
    ddb_ows_gui::init(api);
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

#include "gui/ddb_ows_gui.hpp"

#include <deadbeef/converter.h>
#include <deadbeef/gtkui_api.h>
#include <execinfo.h>
#include <fmt/core.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <set>
#include <string>
#include <thread>

#include "ddb_ows.hpp"
#include "gdk/gdkkeysyms.h"
#include "glibmm/dispatcher.h"
#include "glibmm/refptr.h"
#include "gtkmm/builder.h"
#include "gtkmm/checkbutton.h"
#include "gtkmm/combobox.h"
#include "gtkmm/cssprovider.h"
#include "gtkmm/filechooserbutton.h"
#include "gtkmm/liststore.h"
#include "gtkmm/main.h"
#include "gtkmm/progressbar.h"
#include "gtkmm/spinbutton.h"
#include "gtkmm/stock.h"
#include "gtkmm/textbuffer.h"
#include "gtkmm/textview.h"
#include "gtkmm/togglebutton.h"
#include "gtkmm/treeview.h"
#include "gtkmm/widget.h"
#include "gtkmm/window.h"
#include "playlist_uuid.hpp"

using namespace std::chrono_literals;

static DB_functions_t* ddb;

namespace ddb_ows_gui {

using namespace ddb_ows;

ddb_ows_gui_plugin_t plugin{
    .pm = nullptr,
    .gui_logger = nullptr,
    .sig_execution_buttons_set_sensitive = nullptr,
    .sig_execution_buttons_set_insensitive = nullptr
};

ddb_ows_plugin_t* ddb_ows;

StdioLogger terminal_logger{};

Glib::RefPtr<Gtk::Builder> builder;

std::shared_ptr<spdlog::logger> get_logger() {
    auto logger = spdlog::get(DDB_OWS_GUI_PLUGIN_ID);
    return logger ? logger : spdlog::default_logger();
}

// adapted from boost::hash_combine
size_t hash_combine(size_t x, size_t y) {
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
typedef std::unordered_map<
    ddb_ows_gui::signal_handler_id,
    gulong,
    signal_handler_id_hash>
    signal_map;

typedef struct {
    GModule* gmodule;
    gpointer data;
    signal_map* map;
} connect_args;

signal_map* signals = new signal_map();

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
    connect_args* args = (connect_args*)user_data;

    if (!g_module_symbol(args->gmodule, handler_name, (gpointer*)&func)) {
        g_warning("Could not find signal handler '%s'", handler_name);
        return;
    }

    gulong id;
    if (connect_object) {
        id = g_signal_connect_object(
            object, signal_name, func, connect_object, flags
        );
    } else {
        id = g_signal_connect_data(
            object, signal_name, func, args->data, nullptr, flags
        );
    }

    if (args->map != nullptr) {
        get_logger()->debug(
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

void list_store_check_consistent(
    Glib::RefPtr<Gtk::ListStore> model, Gtk::CheckButton* toggle, int col = 0
) {
    // Check if the bool values in column col of model are all true or all false
    // Update the toggle's inconsistent state accordingly
    bool all_true = true;
    bool all_false = true;
    bool pl_selected;

    auto logger = get_logger();

    if (!model) {
        logger->warn("Attempt to check consistency with null model.");
        return;
    }
    if (!toggle) {
        logger->warn("Attempt to check consistency with null toggle.");
        return;
    }
    auto rows = model->children();
    if (!std::size(rows)) {
        return;
    }
    for (auto r : rows) {
        r->get_value(0, pl_selected);
        all_true = all_true && pl_selected;
        all_false = all_false && !pl_selected;
    }
    toggle->set_inconsistent(!(all_true || all_false));
    toggle->set_active(all_true);
}

void fn_formats_save(Glib::RefPtr<Gtk::ListStore> model) {
    std::vector<std::string> fmts{};
    model->foreach_iter([&fmts](const Gtk::TreeIter r) -> bool {
        std::string f;
        r->get_value(0, f);
        fmts.push_back(f);
        return false;
    });
    ddb_ows->conf->set_fn_formats(fmts);
}

void fn_formats_populate(Glib::RefPtr<Gtk::ListStore> model) {
    std::vector<std::string> fmts = ddb_ows->conf->get_fn_formats();
    if (!fmts.size()) {
        // No formats save in config => use formats from .ui file
        // This has the side effect of bootstrapping the user's config from the
        // .ui file
        return;
    }

    // This function *reads* from the configuration and sets the model
    // accordingly. But the model has signal handlers that *write* to the
    // configuration. We need to block them until we are done to not issue
    // configuration writes in incorrect states. Issue: #35
    GObject* const obj = reinterpret_cast<GObject*>(model->gobj());
    const gulong on_change =
        signals->at({obj, "row-changed", "fn_formats_save"});
    const gulong on_insert =
        signals->at({obj, "row-inserted", "fn_formats_save"});
    const gulong on_delete =
        signals->at({obj, "row-deleted", "fn_formats_save_on_delete"});
    g_signal_handler_block(obj, on_change);
    g_signal_handler_block(obj, on_insert);
    g_signal_handler_block(obj, on_delete);

    model->clear();
    Gtk::TreeModel::iterator r;
    auto logger = get_logger();
    for (auto i : fmts) {
        logger->debug("Appending {} to fn formats", i);
        r = model->append();
        r->set_value(0, i);
    }

    g_signal_handler_unblock(obj, on_change);
    g_signal_handler_unblock(obj, on_insert);
    g_signal_handler_unblock(obj, on_delete);
}

char* validate_fn_format(std::string fmt) {
    char* bc = ddb->tf_compile(fmt.c_str());
    Gtk::Image* valid_indicator;
    builder->get_widget("fn_format_valid_indicator", valid_indicator);
    Gtk::BuiltinStockID icon;
    if (bc != nullptr) {
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
    if (format == nullptr) {
        // This should never happen
        return;
    }
    Gtk::Label* preview_label;
    builder->get_widget("fn_format_preview_label", preview_label);
    DB_playItem_t* it = ddb->streamer_get_playing_track();
    if (!it) {
        // Pick a random track from the current playlist
        ddb->pl_lock();
        ddb_playlist_t* plt = ddb->plt_get_curr();
        if (!plt || !ddb->plt_get_item_count(plt, PL_MAIN)) {
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
    auto logger = get_logger();
    ddb_converter_t* enc_plug =
        (ddb_converter_t*)ddb->plug_get_for_id("converter");
    if (enc_plug == nullptr) {
        logger->warn("Converter plugin not present!");
        return;
    }
    ddb_encoder_preset_t* enc = enc_plug->encoder_preset_get_list();
    Gtk::TreeModel::iterator row;
    Gtk::ComboBox* cp_combobox;
    builder->get_widget("cp_combobox", cp_combobox);
    std::string conf_preset_name = ddb_ows->conf->get_conv_preset();
    while (enc != nullptr) {
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
    auto logger = get_logger();
    logger->debug("Clearing playlist selection model.");

    model->foreach_iter([](const Gtk::TreeIter r) -> bool {
        ddb_playlist_t* plt;
        r->get_value(2, plt);
        ddb->plt_unref(plt);
        return false;
    });
    model->clear();
}

void pl_selection_save(Glib::RefPtr<Gtk::ListStore> model) {
    std::unordered_set<plt_uuid> pls{};
    model->foreach_iter([&pls](const Gtk::TreeIter r) -> bool {
        bool checked;
        r->get_value(0, checked);
        ddb_playlist_t* p;
        r->get_value(2, p);
        if (checked && p != nullptr) {
            pls.insert(ddb_ows->plt_get_uuid(p));
        }
        return false;
    });
    ddb_ows->conf->set_pl_selection(pls);
}
void pl_selection_populate(
    Glib::RefPtr<Gtk::ListStore> model,
    std::unordered_set<plt_uuid> selected_uuids = {}
) {
    auto logger = get_logger();

    int plt_count = ddb->plt_get_count();
    logger->debug(
        "Populating playlist selection model with {} playlists.", plt_count
    );

    GObject* const obj = reinterpret_cast<GObject*>(model->gobj());
    const gulong on_change =
        signals->at({obj, "row-changed", "pl_selection_save"});
    g_signal_handler_block(obj, on_change);

    char buf[4096];
    ddb_playlist_t* plt;
    Gtk::TreeModel::iterator row;
    pl_selection_clear(model);
    for (int i = 0; i < plt_count; i++) {
        plt = ddb->plt_get_for_idx(i);
        ddb->plt_get_title(plt, buf, sizeof(buf));
        row = model->append();
        plt_uuid uuid = ddb_ows->plt_get_uuid(plt);
        bool s = selected_uuids.count(uuid) > 0;
        row->set_value(0, s);
        row->set_value(1, std::string(buf));
        row->set_value(2, plt);
    }

    g_signal_handler_unblock(obj, on_change);
}

void pl_selection_update_model(Glib::RefPtr<Gtk::ListStore> model) {
    // store each playlist's selection status in a map
    std::unordered_set<plt_uuid> selected_uuids = {};
    ddb->pl_lock();
    model->foreach_iter([&selected_uuids](const Gtk::TreeIter r) -> bool {
        bool s;
        r->get_value(0, s);
        ddb_playlist_t* p;
        r->get_value(2, p);
        selected_uuids.insert(ddb_ows->plt_get_uuid(p));
        return false;
    });
    // now rebuild the model using the map to assign selection statuses
    pl_selection_populate(model, selected_uuids);
    ddb->pl_unlock();
}

std::vector<ddb_playlist_t*> get_selected_playlists() {
    auto pl_model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("pl_selection_model")
    );
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

void warn_converter() {
    const auto fts = ddb_ows->conf->get_conv_fts();
    Gtk::Box* box;
    builder->get_widget("warn_converter_box", box);
    if (box != nullptr) {
        bool converter_available = ddb->plug_get_for_id("converter") != nullptr;
        box->set_visible(fts.size() > 0 && !converter_available);
    }
}

void conv_fts_save(Glib::RefPtr<Gtk::ListStore> model) {
    std::set<std::string> fts{};
    model->foreach_iter([&fts](const Gtk::TreeIter r) -> bool {
        std::string name;
        bool checked;
        r->get_value(0, checked);
        r->get_value(1, name);
        if (checked) {
            fts.insert(name);
        }
        return false;
    });
    ddb_ows->conf->set_conv_fts(fts);

    warn_converter();
}

void conv_fts_populate(
    Glib::RefPtr<Gtk::ListStore> model,
    std::unordered_map<std::string, bool> selected = {}
) {
    auto logger = get_logger();

    GObject* const obj = reinterpret_cast<GObject*>(model->gobj());
    const gulong on_change = signals->at({obj, "row-changed", "conv_fts_save"});
    g_signal_handler_block(obj, on_change);

    // decoders and decoders[i]->exts are null-terminated arrays
    DB_decoder_t** decoders = ddb->plug_get_decoder_list();
    int i = 0;
    std::string::size_type n;
    Gtk::TreeModel::iterator row;
    std::set<std::string> sels = ddb_ows->conf->get_conv_fts();

    while (decoders[i]) {
        row = model->append();
        std::string s(decoders[i]->plugin.name);
        if ((n = s.find(" decoder")) != std::string::npos ||
            (n = s.find(" player")) != std::string::npos)
        {
            s = s.substr(0, n);
        }
        row->set_value(0, sels.count(s) > 0);
        row->set_value(1, s);
        row->set_value(2, decoders[i]);
        i++;
    }

    g_signal_handler_unblock(obj, on_change);

    logger->debug("Finished reading decoders.");
}

void loglevel_cb_populate(std::shared_ptr<TextBufferLogger> logger) {
    Gtk::ComboBox* cb;
    builder->get_widget("loglevel_cb", cb);
    if (!cb) {
        return;
    }
    auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(cb->get_model());
    for (auto l : logger->get_levels()) {
        auto row = model->append();
        if (!cb->get_active()) {
            cb->set_active(row);
        }
        row->set_value(0, l.second.name);
        row->set_value(1, l.second.color);
        row->set_value(2, (unsigned int)l.first);
    }
}

/* BEGIN EXTERN SIGNAL HANDLERS */
// TODO consider moving these into their own file

void execute(bool dry) {
    if (plugin.gui_logger) {
        plugin.gui_logger->clear();
    }

    Gtk::ProgressBar* pb;
    builder->get_widget("progress_bar", pb);
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
        job_finished_cb = [pm](std::unique_ptr<Job>, bool) {
            pm->job_finished();
        };
    }

    callback_t callbacks{
        .on_playlist_save = pl_save_cb,
        .on_sources_gathered = sources_gathered_cb,
        .on_job_queued = job_queued_cb,
        .on_queueing_complete = q_complete_cb,
        .on_job_finished = job_finished_cb
    };

    std::vector<ddb_playlist_t*> pls = get_selected_playlists();

    if (plugin.gui_logger) {
        ddb_ows->run(dry, pls, *plugin.gui_logger, callbacks);
    } else {
        ddb_ows->run(dry, pls, terminal_logger, callbacks);
    }
}

void execution_buttons_set_sensitive(bool sensitive) {
    Button* dry_run_btn = nullptr;
    Button* execute_btn = nullptr;
    builder->get_widget("execute_btn", execute_btn);
    builder->get_widget("dry_run_btn", dry_run_btn);
    if (dry_run_btn) {
        dry_run_btn->set_sensitive(sensitive);
    }
    if (execute_btn) {
        execute_btn->set_sensitive(sensitive);
    }
}

void execution_buttons_set_sensitive() {
    execution_buttons_set_sensitive(true);
}
void execution_buttons_set_insensitive() {
    execution_buttons_set_sensitive(false);
}

auto execution_thread(bool dry) {
    return std::thread([dry] {
        execute(dry);
        (*plugin.sig_execution_buttons_set_sensitive)();
    });
}

extern "C" {

void on_select_all_toggled(GtkListStore* ls, gpointer data) {
    // Taking ownership of the instance can lead to incorrect reference counts
    // so we must pass true as the second argument to take a new copy or ref
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    Gtk::CheckButton* toggle = Glib::wrap(
        GTK_CHECK_BUTTON(
            gtk_tree_view_column_get_widget(GTK_TREE_VIEW_COLUMN(data))
        ),
        true
    );
    bool sel = toggle->get_inconsistent() || !toggle->get_active();
    model->foreach_iter([sel, &model](const Gtk::TreeIter r) -> bool {
        r->set_value(0, sel);
        std::string n;
        r->get_value(1, n);
        model->row_changed(Gtk::TreePath(r), r);
        return false;
    });
    toggle->set_active(sel);
}

void on_selected_rend_toggled(
    GtkCellRendererToggle* rend, char* path, gpointer data
) {
    if (data == nullptr) {
        return;
    }
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(GTK_LIST_STORE(data), true);
    auto row = model->get_iter(path);
    bool pl_selected;
    row->get_value(0, pl_selected);
    row->set_value(0, !pl_selected);
    model->row_changed(Gtk::TreePath(path), row);
}

void list_store_check_consistent(
    GtkListStore* ls, GtkTreePath* path, GtkTreeIter* iter, gpointer data
) {
    if (data == nullptr) {
        return;
    }
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    Gtk::CheckButton* toggle = Glib::wrap(GTK_CHECK_BUTTON(data), true);
    list_store_check_consistent(model, toggle);
}

// needed because row-deleted has a different call signature than row-changed
// and row-inserted
void list_store_check_consistent_on_delete(
    GtkListStore* ls, GtkTreePath* path, gpointer data
) {
    list_store_check_consistent(ls, path, nullptr, data);
}

/* UI initalisation -- populate the various ListStores with data from DeadBeeF
 */

void cp_populate(GtkListStore* ls, gpointer data) {
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    cp_populate(model);
}

/* Handle config updates */

void on_target_root_chooser_selection_changed(
    GtkFileChooserButton* fcb, gpointer data
) {
    // We connect to this signal because file-set is only emitted if the file
    // browser was opened, not if the target was chosen from the drop-down
    // menu.
    GFile* root = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(fcb));
    char* root_path = g_file_get_path(root);
    ddb_ows->conf->set_root(std::string(root_path));
    g_free(root_path);
    g_object_unref(root);
}

void fn_formats_save(
    GtkListStore* ls, GtkTreePath* path, GtkTreeIter* iter, gpointer data
) {
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    fn_formats_save(model);
}

// needed because row-deleted has a different call signature than row-changed
// and row-inserted
void fn_formats_save_on_delete(
    GtkListStore* ls, GtkTreePath* path, gpointer data
) {
    fn_formats_save(ls, path, nullptr, data);
}

void on_fn_format_entered(Gtk::Entry* entry) {
    auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("fn_format_model")
    );
    std::string fn_format = entry->get_text();
    char* format = validate_fn_format(fn_format);
    if (format == nullptr) {
        return;
    }
    auto rows = model->children();
    std::string f;
    Gtk::ComboBox* fn_combobox;
    builder->get_widget("fn_format_combobox", fn_combobox);
    for (auto i : rows) {
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
        GtkEntry* entry = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(fn_combobox)));
        fn_format = gtk_entry_get_text(entry);
    }

    char* format = validate_fn_format(fn_format);
    if (format != nullptr) {
        update_fn_preview(format);
    } else {
        clear_fn_preview();
    }
}

void on_loglevel_cb_changed(GtkComboBox* loglevel_cb, gpointer data) {
    if (!plugin.gui_logger) {
        return;
    }
    GtkTreeIter active;
    if (gtk_combo_box_get_active_iter(loglevel_cb, &active) != TRUE) {
        return;
    }
    auto model = gtk_combo_box_get_model(loglevel_cb);
    guint level;
    gtk_tree_model_get(model, &active, 2, &level, -1);
    plugin.gui_logger->set_level((loglevel_e)level);
}

/* Initialize the UI with values from config */

void on_cover_fname_entry_show(GtkWidget* widget, gpointer data) {
    gtk_entry_set_text(
        GTK_ENTRY(widget), ddb_ows->conf->get_cover_fname().c_str()
    );
}

void on_conv_ext_entry_show(GtkWidget* widget, gpointer data) {
    gtk_entry_set_text(
        GTK_ENTRY(widget), ddb_ows->conf->get_conv_ext().c_str()
    );
}

void on_warn_converter_box_show(GtkWidget* widget, gpointer data) {
    warn_converter();
}

void on_target_root_chooser_show(GtkWidget* widget, gpointer data) {
    auto logger = get_logger();
    std::string root = ddb_ows->conf->get_root();
    logger->debug("Setting root to {}", root);
    const char* path = root.c_str();
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(widget), path);
}

void on_fn_format_combobox_show(GtkWidget* widget, gpointer data) {
    GtkListStore* model =
        GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(widget)));
    auto fn_model =
        Glib::RefPtr<Gtk::ListStore>::cast_static(Glib::wrap(model));
    fn_formats_populate(fn_model);
    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);

    Gtk::ComboBox* fn_combobox = Glib::wrap(GTK_COMBO_BOX(widget), true);
    Gtk::Entry* fn_entry = (Gtk::Entry*)fn_combobox->get_child();
    fn_entry->signal_activate().connect(
        sigc::bind(sigc::ptr_fun(&on_fn_format_entered), fn_entry)
    );
    fn_entry->signal_focus_out_event().connect(
        sigc::bind(sigc::ptr_fun(&on_fn_format_focus_out), fn_entry)
    );
}

void on_cover_sync_check_show(GtkWidget* widget, gpointer data) {
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(widget), ddb_ows->conf->get_cover_sync()
    );
}

void on_cover_timeout_spinbutton_show(GtkWidget* widget, gpointer data) {
    GtkAdjustment* adjustment =
        gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(widget));
    unsigned int timeout_ms = ddb_ows->conf->get_cover_timeout_ms();
    gtk_adjustment_set_value(adjustment, timeout_ms);
}

void on_sync_pls_dbpl_check_show(GtkWidget* widget, gpointer data) {
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(widget), ddb_ows->conf->get_sync_pls().dbpl
    );
}

void on_sync_pls_m3u8_check_show(GtkWidget* widget, gpointer data) {
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(widget), ddb_ows->conf->get_sync_pls().m3u8
    );
}

void on_rm_unref_check_show(GtkWidget* widget, gpointer data) {
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(widget), ddb_ows->conf->get_rm_unref()
    );
}

void on_wt_spinbutton_show(GtkWidget* widget, gpointer data) {
    gtk_spin_button_set_value(
        GTK_SPIN_BUTTON(widget), ddb_ows->conf->get_conv_wts()
    );
}

/* Save values to config when changed in the UI */

void pl_selection_save(
    GtkListStore* ls, GtkTreePath* path, GtkTreeIter* iter, gpointer data
) {
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    pl_selection_save(model);
}

void on_cover_fname_entry_changed(GtkEntry* entry, gpointer data) {
    const gchar* cover_fname = gtk_entry_get_text(entry);
    ddb_ows->conf->set_cover_fname(std::string(cover_fname));
}

void on_cover_timeout_spinbutton_value_changed(
    GtkSpinButton* timeout, gpointer data
) {
    GtkAdjustment* adjustment;

    adjustment = gtk_spin_button_get_adjustment(timeout);
    unsigned int timeout_ms = gtk_adjustment_get_value(adjustment);
    ddb_ows->conf->set_cover_timeout_ms(timeout_ms);
}

void warn_artwork() {
    Gtk::Box* box;
    builder->get_widget("warn_artwork_box", box);
    auto cover_sync = ddb_ows->conf->get_cover_sync();
    if (box != nullptr) {
        bool artwork_available = ddb->plug_get_for_id("artwork2") != nullptr;
        box->set_visible(cover_sync && !artwork_available);
    }
}

void on_warn_artwork_box_show(GtkWidget*, gpointer) { warn_artwork(); }

void on_cover_sync_check_toggled(GtkToggleButton* toggle, gpointer data) {
    gboolean cover_sync = gtk_toggle_button_get_active(toggle);
    ddb_ows->conf->set_cover_sync(cover_sync);
}

void on_sync_pls_dbpl_check_toggled(GtkToggleButton* toggle, gpointer data) {
    gboolean sync_pls = gtk_toggle_button_get_active(toggle);
    auto s = ddb_ows->conf->get_sync_pls();
    s.dbpl = sync_pls;
    ddb_ows->conf->set_sync_pls(s);
}

void on_sync_pls_m3u8_check_toggled(GtkToggleButton* toggle, gpointer data) {
    gboolean sync_pls = gtk_toggle_button_get_active(toggle);
    auto s = ddb_ows->conf->get_sync_pls();
    s.m3u8 = sync_pls;
    ddb_ows->conf->set_sync_pls(s);
}

void on_rm_unref_check_toggled(GtkToggleButton* toggle, gpointer data) {
    gboolean rm_unref = gtk_toggle_button_get_active(toggle);
    ddb_ows->conf->set_rm_unref(rm_unref);
}

void on_wt_spinbutton_value_changed(GtkSpinButton* spinbutton, gpointer data) {
    int wt = (int)gtk_spin_button_get_value(spinbutton);
    ddb_ows->conf->set_conv_wts(wt);
}

void conv_fts_save(
    GtkListStore* ls, GtkTreePath* path, GtkTreeIter* iter, gpointer data
) {
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    conv_fts_save(model);
}

void on_conv_ext_entry_changed(GtkEntry* entry, gpointer data) {
    const gchar* conv_ext = gtk_entry_get_text(entry);
    ddb_ows->conf->set_conv_ext(std::string(conv_ext));
}

void on_cp_combobox_changed(GtkComboBox* combobox, gpointer data) {
    auto logger = get_logger();
    logger->debug("Preset changed.");
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(GTK_LIST_STORE(data), true);
    Gtk::ComboBox* cb = Glib::wrap(combobox, true);
    auto iter = cb->get_active();
    std::string out;
    iter->get_value(0, out);
    ddb_ows->conf->set_conv_preset(out);
}

/* Clean-up actions */

void pl_selection_clear(GtkListStore* ls, gpointer data) {
    auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(Glib::wrap(ls));
    pl_selection_clear(model);
}

/* Button actions */

void on_quit_btn_clicked() {
    GtkWidget* cwin =
        GTK_WIDGET(gtk_builder_get_object(builder->gobj(), "ddb_ows"));
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
    execution_thread(dry).detach();
}

void on_dry_run_btn_clicked(GtkButton* button, gpointer data) {
    on_execution_btn_clicked(true);
}

void on_execute_btn_clicked(GtkButton* button, gpointer data) {
    on_execution_btn_clicked(false);
}

gboolean on_ddb_ows_key_press_event(
    GtkWidget* widget, GdkEventKey* key, gpointer data
) {
    if (key->keyval == GDK_KEY_Escape) {
        gtk_widget_hide(widget);
        return TRUE;
    }
    return FALSE;
}

/* END EXTERN SIGNAL HANDLERS */
}

int create_ui() {
    auto logger = get_logger();
    try {
        builder->add_from_resource("/ddb_ows/ddb_ows.ui");
    } catch (Gtk::BuilderError& e) {
        logger->error("Could not build ui: {}.", std::string(e.what()));
        return -1;
    }
    auto provider = Gtk::CssProvider::create();
    provider->load_from_resource("/ddb_ows/ddb_ows.css");
    Gtk::StyleContext::add_provider_for_screen(
        Gdk::Screen::get_default(),
        provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    // Use introspection (backtrace) to figure out which file (.so) we are in.
    // This is necessary because we have to tell gtk to look for the signal
    // handlers in the .so, rather than in the main deadbeef executable.
    char** bt_symbols = nullptr;
    void* trace[1];
    int trace_l = backtrace(trace, 1);
    bt_symbols = backtrace_symbols(trace, trace_l);
    // Backtrace looks something like
    // "/usr/lib/deadbeef/ddb_ows_gtk3.so(create_ui+0x53) [0x7f7b0ae932a3]"
    // We need to account for the possibility that the path is silly and
    // contains '(' => find last position
    char* last_bracket = strrchr(bt_symbols[0], '(');
    if (last_bracket) {
        *last_bracket = '\0';
    }
    logger->debug("Trying to load GModule {}", bt_symbols[0]);

    // Now we are ready to connect signal handlers;
    connect_args* args = g_slice_new0(connect_args);
    args->gmodule = g_module_open(bt_symbols[0], G_MODULE_BIND_LAZY);
    args->data = nullptr;
    args->map = signals;
    gtk_builder_connect_signals_full(
        builder->gobj(), gtk_builder_connect_signals_default, args
    );

    plugin.sig_execution_buttons_set_sensitive =
        std::make_shared<Glib::Dispatcher>();
    plugin.sig_execution_buttons_set_sensitive->connect(
        sigc::ptr_fun<void>(execution_buttons_set_sensitive)
    );
    plugin.sig_execution_buttons_set_insensitive =
        std::make_shared<Glib::Dispatcher>();
    plugin.sig_execution_buttons_set_insensitive->connect(
        sigc::ptr_fun<void>(execution_buttons_set_insensitive)
    );

    auto pl_model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("pl_selection_model")
    );
    pl_selection_populate(pl_model, ddb_ows->conf->get_pl_selection());

    auto ft_model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("ft_model")
    );
    conv_fts_populate(ft_model);

    auto cp_model = Glib::RefPtr<Gtk::ListStore>::cast_static(
        builder->get_object("cp_model")
    );
    cp_populate(cp_model);

    Gtk::TextView* job_log;
    builder->get_widget("job_log", job_log);
    if (job_log) {
        auto log_buffer = Glib::RefPtr<Gtk::TextBuffer>::cast_static(
            builder->get_object("job_log_buffer")
        );
        plugin.gui_logger =
            std::make_shared<TextBufferLogger>(log_buffer, job_log);
        log_buffer->create_mark("END", log_buffer->end(), false);

        loglevel_cb_populate(plugin.gui_logger);
    }

    return 0;
}

int show_ui(DB_plugin_action_t* action, ddb_action_context_t ctx) {
    Gtk::Window* ddb_ows_win = nullptr;
    builder->get_widget("ddb_ows", ddb_ows_win);
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

    ddb_ows = (ddb_ows_plugin_t*)ddb->plug_get_for_id("ddb_ows");
    if (!ddb_ows) {
        logger->error("ddb_ows plugin not found, quitting.");
        return -1;
    }

    DB_plugin_t* ddb_gtkui = ddb->plug_get_for_id(DDB_GTKUI_PLUGIN_ID);
    if (!ddb_gtkui) {
        logger->error("Matching gtkui plugin not found, quitting.");
        return -1;
    }
    // Needed to make gtkmm play nice
    auto __attribute__((unused)) app = new Gtk::Main(0, nullptr, false);
    builder = Gtk::Builder::create();
    logger->info("Initialized successfully.");
    return create_ui();
}

int disconnect() {
    if (builder) {
        auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(
            builder->get_object("pl_selection_model")
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
    builder->get_widget("fn_format_combobox", fn_combobox);
    switch (id) {
        case DB_EV_PLAYLISTCHANGED:
            if (p1 == DDB_PLAYLIST_CHANGE_CONTENT ||
                p1 == DDB_PLAYLIST_CHANGE_SELECTION ||
                p1 == DDB_PLAYLIST_CHANGE_SEARCHRESULT ||
                p1 == DDB_PLAYLIST_CHANGE_PLAYQUEUE)
            {
                break;
            }
            model = Glib::RefPtr<Gtk::ListStore>::cast_static(
                builder->get_object("pl_selection_model")
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
    ddb = api;
    ddb_ows_gui::init(api);
    auto logger = spdlog::stderr_color_mt(DDB_OWS_GUI_PLUGIN_ID);
    logger->set_level(spdlog::level::DDB_OWS_LOGLEVEL);
    logger->set_pattern("[%n] [%^%l%$] [thread %t] %v");
    return (DB_plugin_t*)&ddb_ows_gui::plugin;
}

extern "C" DB_plugin_t*
#if GTK_CHECK_VERSION(3, 0, 0)
ddb_ows_gtk3_load(DB_functions_t* api) {
#else
#error "Only Gtk 3 is supported!"
#endif
    return load(api);
}

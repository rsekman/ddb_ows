#include <gtkmm/combobox.h>
#include <gtkmm/liststore.h>
#include <gtkmm/stock.h>

#include <string>

#include "gui/internal.hpp"

namespace ddb_ows_gui {

// tf_ptr: unique_ptr for titleformat bytecode with custom deleter
using tf_free_fn = decltype(DB_functions_t::tf_free);
using tf_ptr = std::unique_ptr<char, tf_free_fn>;

tf_ptr validate_fn_format(std::string fmt) {
    tf_ptr bc(ddb->tf_compile(fmt.c_str()), ddb->tf_free);
    Gtk::Image* valid_indicator;
    plugin.builder->get_widget("fn_format_valid_indicator", valid_indicator);
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
    plugin.builder->get_widget("fn_format_preview_label", preview_label);
    preview_label->set_text("");
}

void update_fn_preview(char* format) {
    if (format == nullptr) {
        // This should never happen
        return;
    }
    Gtk::Label* preview_label;
    plugin.builder->get_widget("fn_format_preview_label", preview_label);
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
    ddb->pl_item_unref(it);
    preview_label->set_text(out);
    preview_label->queue_resize();
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
        // No formats saved in config => use formats from .ui file
        // This has the side effect of bootstrapping the user's config from the
        // .ui file
        return;
    }

    // This function *reads* from the configuration and sets the model
    // accordingly. But the model has signal handlers that *write* to the
    // configuration. We need to block them until we are done to not issue
    // configuration writes in incorrect states. Issue: #35
    GObject* const obj = reinterpret_cast<GObject*>(model->gobj());
    const gulong on_change = plugin.signals->at({obj, "row-changed", "fn_formats_save"});
    const gulong on_insert = plugin.signals->at({obj, "row-inserted", "fn_formats_save"});
    const gulong on_delete = plugin.signals->at({obj, "row-deleted", "fn_formats_save_on_delete"});
    g_signal_handler_block(obj, on_change);
    g_signal_handler_block(obj, on_insert);
    g_signal_handler_block(obj, on_delete);

    model->clear();
    Gtk::TreeModel::iterator r;
    auto logger = get_logger();
    for (auto i : fmts) {
        logger->trace("Appending {} to fn formats", i);
        r = model->append();
        r->set_value(0, i);
    }

    g_signal_handler_unblock(obj, on_change);
    g_signal_handler_unblock(obj, on_insert);
    g_signal_handler_unblock(obj, on_delete);
}

// sigc-connected handlers (not extern "C")

void on_fn_format_entered(Gtk::Entry* entry) {
    auto model =
        Glib::RefPtr<Gtk::ListStore>::cast_static(plugin.builder->get_object("fn_format_model"));
    std::string fn_format = entry->get_text();
    const auto format = validate_fn_format(fn_format);
    if (format == nullptr) {
        return;
    }
    auto rows = model->children();
    std::string f;
    Gtk::ComboBox* fn_combobox;
    plugin.builder->get_widget("fn_format_combobox", fn_combobox);
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
    update_fn_preview(format.get());
}

bool on_fn_format_focus_out(GdkEventFocus* event, Gtk::Entry* entry) {
    on_fn_format_entered(entry);
    return false;
}

extern "C" {

void fn_formats_save(GtkListStore* ls, GtkTreePath* path, GtkTreeIter* iter, gpointer data) {
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    fn_formats_save(model);
}

// needed because row-deleted has a different call signature than row-changed
// and row-inserted
void fn_formats_save_on_delete(GtkListStore* ls, GtkTreePath* path, gpointer data) {
    fn_formats_save(ls, path, nullptr, data);
}

void on_fn_format_combobox_changed(GtkComboBox* fn_combobox, gpointer data) {
    std::string fn_format;
    gint active = gtk_combo_box_get_active(fn_combobox);
    if (active >= 0) {
        auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(
            plugin.builder->get_object("fn_format_model")
        );
        auto rows = model->children();
        rows[active]->get_value(0, fn_format);
        model->move(rows[active], rows.begin());
    } else {
        GtkEntry* entry = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(fn_combobox)));
        fn_format = gtk_entry_get_text(entry);
    }

    const auto format = validate_fn_format(fn_format);
    if (format != nullptr) {
        update_fn_preview(format.get());
    } else {
        clear_fn_preview();
    }
}

void on_fn_format_combobox_show(GtkWidget* widget, gpointer data) {
    GtkListStore* model = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(widget)));
    auto fn_model = Glib::RefPtr<Gtk::ListStore>::cast_static(Glib::wrap(model));
    fn_formats_populate(fn_model);
    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);

    Gtk::ComboBox* fn_combobox = Glib::wrap(GTK_COMBO_BOX(widget), true);
    auto* fn_entry = static_cast<Gtk::Entry*>(fn_combobox->get_child());
    fn_entry->signal_activate().connect(sigc::bind(sigc::ptr_fun(&on_fn_format_entered), fn_entry));
    fn_entry->signal_focus_out_event().connect(
        sigc::bind(sigc::ptr_fun(&on_fn_format_focus_out), fn_entry)
    );
}

}  // extern "C"

}  // namespace ddb_ows_gui

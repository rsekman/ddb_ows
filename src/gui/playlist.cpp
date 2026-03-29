#include <gtkmm/checkbutton.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treeview.h>

#include <string>

#include "gui/internal.hpp"

namespace ddb_ows_gui {

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
    Glib::RefPtr<Gtk::ListStore> model, std::unordered_set<plt_uuid> selected_uuids
) {
    auto logger = get_logger();

    int plt_count = ddb->plt_get_count();
    logger->debug("Populating playlist selection model with {} playlists.", plt_count);

    GObject* const obj = reinterpret_cast<GObject*>(model->gobj());
    const gulong on_change = plugin.signals->at({obj, "row-changed", "pl_selection_save"});
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
        bool s = selected_uuids.contains(uuid);
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

extern "C" {

void on_select_all_toggled(GtkListStore* ls, gpointer data) {
    // Taking ownership of the instance can lead to incorrect reference counts
    // so we must pass true as the second argument to take a new copy or ref
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    Gtk::CheckButton* toggle = Glib::wrap(
        GTK_CHECK_BUTTON(gtk_tree_view_column_get_widget(GTK_TREE_VIEW_COLUMN(data))), true
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

void on_selected_rend_toggled(GtkCellRendererToggle* rend, char* path, gpointer data) {
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
void list_store_check_consistent_on_delete(GtkListStore* ls, GtkTreePath* path, gpointer data) {
    list_store_check_consistent(ls, path, nullptr, data);
}

void pl_selection_save(GtkListStore* ls, GtkTreePath* path, GtkTreeIter* iter, gpointer data) {
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    pl_selection_save(model);
}

void pl_selection_clear(GtkListStore* ls, gpointer data) {
    auto model = Glib::RefPtr<Gtk::ListStore>::cast_static(Glib::wrap(ls));
    pl_selection_clear(model);
}

}  // extern "C"

}  // namespace ddb_ows_gui

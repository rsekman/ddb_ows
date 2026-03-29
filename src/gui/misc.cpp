#include <gtkmm/combobox.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/liststore.h>
#include <gtkmm/togglebutton.h>

#include <gui/ddb_ows_gui.hpp>
#include <gui/internal.hpp>

namespace ddb_ows_gui {

void loglevel_cb_populate(std::shared_ptr<TextBufferLogger> logger) {
    Gtk::ComboBox* cb;
    plugin.builder->get_widget("loglevel_cb", cb);
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
        row->set_value(2, static_cast<unsigned int>(l.first));
    }
}

extern "C" {

void on_target_root_chooser_selection_changed(GtkFileChooserButton* fcb, gpointer data) {
    // We connect to this signal because file-set is only emitted if the file
    // browser was opened, not if the target was chosen from the drop-down
    // menu.
    GFile* root = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(fcb));
    char* root_path = g_file_get_path(root);
    ddb_ows->conf->set_root(std::string(root_path));
    g_free(root_path);
    g_object_unref(root);
}

void on_target_root_chooser_show(GtkWidget* widget, gpointer data) {
    auto logger = get_logger();
    std::string root = ddb_ows->conf->get_root();
    logger->debug("Setting root to {}", root);
    const char* path = root.c_str();
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(widget), path);
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
    plugin.gui_logger->set_level(static_cast<loglevel_e>(level));
}

void on_rm_unref_check_show(GtkWidget* widget, gpointer data) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), ddb_ows->conf->get_rm_unref());
}

void on_rm_unref_check_toggled(GtkToggleButton* toggle, gpointer data) {
    gboolean rm_unref = gtk_toggle_button_get_active(toggle);
    ddb_ows->conf->set_rm_unref(rm_unref);
}

void on_sync_pls_dbpl_check_show(GtkWidget* widget, gpointer data) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), ddb_ows->conf->get_sync_pls().dbpl);
}

void on_sync_pls_dbpl_check_toggled(GtkToggleButton* toggle, gpointer data) {
    gboolean sync_pls = gtk_toggle_button_get_active(toggle);
    auto s = ddb_ows->conf->get_sync_pls();
    s.dbpl = sync_pls;
    ddb_ows->conf->set_sync_pls(s);
}

void on_sync_pls_m3u8_check_show(GtkWidget* widget, gpointer data) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), ddb_ows->conf->get_sync_pls().m3u8);
}

void on_sync_pls_m3u8_check_toggled(GtkToggleButton* toggle, gpointer data) {
    gboolean sync_pls = gtk_toggle_button_get_active(toggle);
    auto s = ddb_ows->conf->get_sync_pls();
    s.m3u8 = sync_pls;
    ddb_ows->conf->set_sync_pls(s);
}

}  // extern "C"

}  // namespace ddb_ows_gui

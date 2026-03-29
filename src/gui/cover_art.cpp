#include <gtkmm/box.h>
#include <gtkmm/togglebutton.h>

#include "gui/internal.hpp"

namespace ddb_ows_gui {

void warn_artwork() {
    Gtk::Box* box;
    plugin.builder->get_widget("warn_artwork_box", box);
    auto cover_sync = ddb_ows->conf->get_cover_sync();
    if (box != nullptr) {
        bool artwork_available = ddb->plug_get_for_id("artwork2") != nullptr;
        box->set_visible(cover_sync && !artwork_available);
    }
}

extern "C" {

void on_warn_artwork_box_show(GtkWidget*, gpointer) { warn_artwork(); }

void on_cover_fname_entry_show(GtkWidget* widget, gpointer data) {
    gtk_entry_set_text(GTK_ENTRY(widget), ddb_ows->conf->get_cover_fname().c_str());
}

void on_cover_fname_entry_changed(GtkEntry* entry, gpointer data) {
    const gchar* cover_fname = gtk_entry_get_text(entry);
    ddb_ows->conf->set_cover_fname(std::string(cover_fname));
}

void on_cover_sync_check_show(GtkWidget* widget, gpointer data) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), ddb_ows->conf->get_cover_sync());
}

void on_cover_sync_check_toggled(GtkToggleButton* toggle, gpointer data) {
    gboolean cover_sync = gtk_toggle_button_get_active(toggle);
    ddb_ows->conf->set_cover_sync(cover_sync);
}

void on_cover_timeout_spinbutton_show(GtkWidget* widget, gpointer data) {
    GtkAdjustment* adjustment = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(widget));
    unsigned int timeout_ms = ddb_ows->conf->get_cover_timeout_ms();
    gtk_adjustment_set_value(adjustment, timeout_ms);
}

void on_cover_timeout_spinbutton_value_changed(GtkSpinButton* timeout, gpointer data) {
    GtkAdjustment* adjustment;

    adjustment = gtk_spin_button_get_adjustment(timeout);
    unsigned int timeout_ms = gtk_adjustment_get_value(adjustment);
    ddb_ows->conf->set_cover_timeout_ms(timeout_ms);
}

}  // extern "C"

}  // namespace ddb_ows_gui

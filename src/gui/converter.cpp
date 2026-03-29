// clang-format off: includes must be in this order
#include <deadbeef/deadbeef.h>
#include <deadbeef/converter.h>
// clang-format on

#include <gtkmm/box.h>
#include <gtkmm/combobox.h>
#include <gtkmm/liststore.h>

#include <set>
#include <string>

#include "gui/internal.hpp"

namespace ddb_ows_gui {

void warn_converter() {
    const auto fts = ddb_ows->conf->get_conv_fts();
    Gtk::Box* box;
    plugin.builder->get_widget("warn_converter_box", box);
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
    Glib::RefPtr<Gtk::ListStore> model, std::unordered_map<std::string, bool> selected
) {
    auto logger = get_logger();

    GObject* const obj = reinterpret_cast<GObject*>(model->gobj());
    const gulong on_change = plugin.signals->at({obj, "row-changed", "conv_fts_save"});
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
        row->set_value(0, sels.contains(s));
        row->set_value(1, s);
        row->set_value(2, decoders[i]);
        i++;
    }

    g_signal_handler_unblock(obj, on_change);

    logger->debug("Finished reading decoders.");
}

void cp_populate(Glib::RefPtr<Gtk::ListStore> model) {
    auto logger = get_logger();
    auto* enc_plug = reinterpret_cast<ddb_converter_t*>(ddb->plug_get_for_id("converter"));
    if (enc_plug == nullptr) {
        logger->warn("Converter plugin not present!");
        return;
    }
    ddb_encoder_preset_t* enc = enc_plug->encoder_preset_get_list();
    Gtk::TreeModel::iterator row;
    Gtk::ComboBox* cp_combobox;
    plugin.builder->get_widget("cp_combobox", cp_combobox);
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

extern "C" {

void cp_populate(GtkListStore* ls, gpointer data) {
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    cp_populate(model);
}

void on_warn_converter_box_show(GtkWidget* widget, gpointer data) { warn_converter(); }

void conv_fts_save(GtkListStore* ls, GtkTreePath* path, GtkTreeIter* iter, gpointer data) {
    Glib::RefPtr<Gtk::ListStore> model = Glib::wrap(ls, true);
    conv_fts_save(model);
}

void on_conv_ext_entry_show(GtkWidget* widget, gpointer data) {
    gtk_entry_set_text(GTK_ENTRY(widget), ddb_ows->conf->get_conv_ext().c_str());
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

void on_wt_spinbutton_show(GtkWidget* widget, gpointer data) {
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), ddb_ows->conf->get_conv_wts());
}

void on_wt_spinbutton_value_changed(GtkSpinButton* spinbutton, gpointer data) {
    int wt = static_cast<int>(gtk_spin_button_get_value(spinbutton));
    ddb_ows->conf->set_conv_wts(wt);
}

}  // extern "C"

}  // namespace ddb_ows_gui

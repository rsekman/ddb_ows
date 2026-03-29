#ifndef DDB_OWS_GUI_CONVERTER_H
#define DDB_OWS_GUI_CONVERTER_H

#include <gtkmm/liststore.h>

namespace ddb_ows_gui {

void conv_fts_populate(
    Glib::RefPtr<Gtk::ListStore> model, std::unordered_map<std::string, bool> selected = {}
);
void cp_populate(Glib::RefPtr<Gtk::ListStore> model);

}  // namespace ddb_ows_gui

#endif

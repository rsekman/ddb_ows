#ifndef DDB_OWS_GUI_PLAYLIST_H
#define DDB_OWS_GUI_PLAYLIST_H

#include <gtkmm/liststore.h>

#include <unordered_set>

#include "playlist_uuid.hpp"

namespace ddb_ows_gui {

void pl_selection_populate(
    Glib::RefPtr<Gtk::ListStore> model, std::unordered_set<ddb_ows::plt_uuid> selected_uuids = {}
);
void pl_selection_update_model(Glib::RefPtr<Gtk::ListStore> model);
void pl_selection_clear(Glib::RefPtr<Gtk::ListStore> model);

}  // namespace ddb_ows_gui

#endif

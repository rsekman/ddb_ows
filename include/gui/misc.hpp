#ifndef DDB_OWS_GUI_MISC_H
#define DDB_OWS_GUI_MISC_H

#include <gtkmm/liststore.h>

#include "gui/textbufferlogger.hpp"

namespace ddb_ows_gui {

void loglevel_cb_populate(std::shared_ptr<ddb_ows::TextBufferLogger> logger);

}  // namespace ddb_ows_gui

#endif

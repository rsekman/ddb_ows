#ifndef DDB_OWS_GUI_H
#define DDB_OWS_GUI_H

#include <deadbeef/deadbeef.h>
#include <gtk/gtk.h>

#include "progressmonitor.hpp"
#include "textbufferlogger.hpp"

#if GTK_CHECK_VERSION(3, 0, 0)
#define DDB_OWS_GUI_PLUGIN_ID "ddb_ows_gtk3"
#define DDB_OWS_GUI_PLUGIN_NAME "ddb_ows_gtk3"
#else
#error "Only Gtk 3 is supported!"
#endif

#define DDB_OWS_GUI_GLADE "ddb_ows.ui"

#endif

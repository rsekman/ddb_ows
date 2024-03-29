#ifndef DDB_OWS_GUI_H
#define DDB_OWS_GUI_H

#include <gtk/gtk.h>

#include <deadbeef/deadbeef.h>

#include "progressmonitor.hpp"
#include "textbufferlogger.hpp"

#if GTK_CHECK_VERSION(3,0,0)
#define DDB_OWS_GUI_PLUGIN_ID "ddb_ows_gtk3"
#define DDB_OWS_GUI_PLUGIN_NAME "ddb_ows_gtk3"
#else
#define DDB_OWS_GUI_PLUGIN_ID "ddb_ows_gtk2"
#define DDB_OWS_GUI_PLUGIN_NAME "ddb_ows_gtk2"
#endif

#define DDB_OWS_GUI_GLADE "ddb_ows.ui"

typedef struct {
    DB_misc_t plugin;
    ProgressMonitor* pm;
    TextBufferLogger* gui_logger;

} ddb_ows_gui_plugin_t;

#endif

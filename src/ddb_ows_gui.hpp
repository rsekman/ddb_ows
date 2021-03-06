#ifndef DDB_OWS_GUI_H
#define DDB_OWS_GUI_H
#include <gtk/gtk.h>

#include <deadbeef/deadbeef.h>
#include <deadbeef/plugins/converter/converter.h>
#include <deadbeef/plugins/gtkui/gtkui_api.h>

#if GTK_CHECK_VERSION(3,0,0)
#define DDB_OWS_GUI_PLUGIN_ID "ddb_ows_gtk3"
#define DDB_OWS_GUI_PLUGIN_NAME "ddb_ows_gtk3"
#else
#define DDB_OWS_GUI_PLUGIN_ID "ddb_ows_gtk2"
#define DDB_OWS_GUI_PLUGIN_NAME "ddb_ows_gtk2"
#endif

#define DDB_OWS_GUI_GLADE "ddb_ows.ui"


#endif

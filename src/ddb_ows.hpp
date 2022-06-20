#ifndef DDB_OWS_PROJECT_ID

#include <string>

#include "deadbeef/deadbeef.h"

#include "config.hpp"
#include "log.hpp"

#define DDB_OWS_VERSION_MAJOR 0
#define DDB_OWS_VERSION_MINOR 1
#define DDB_OWS_PROJECT_ID "ddb_ows"
#define DDB_OWS_PROJECT_NAME "ddb_ows"
#define DDB_OWS_PROJECT_DESC "Provides One-Way Sync to manage e.g. portable music players. Inspired by foo_ows."
#define DDB_OWS_PROJECT_URL "https://github.com/rsekman/ddb_ows"
#define DDB_OWS_DEFAULT_SOCKET "/tmp/ddb_socket"
#define DDB_OWS_POLL_FREQ 1000 // Socket polling frequency in ms
#define DDB_OWS_MAX_MESSAGE_LENGTH 4096
#define DDB_OWS_LICENSE_TEXT  \
    "Copyright 2021 Robin Ekman\n" \
    "\n" \
    "This program is free software: you can redistribute it and/or modify\n" \
    "it under the terms of the GNU General Public License as published by\n" \
    "the Free Software Foundation, either version 3 of the License, or\n" \
    "(at your option) any later version.\n" \
    "\n" \
    "This program is distributed in the hope that it will be useful,\n" \
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n" \
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n" \
    "GNU General Public License for more details.\n" \
    "\n" \
    "You should have received a copy of the GNU General Public License\n" \
    "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n"

typedef struct {
    DB_misc_t plugin;
    ddb_ows::Configuration& conf;
    std::string (*get_output_path)(DB_playItem_t* it, char* format);
} ddb_ows_plugin_t;

#endif

#ifndef DDB_OWS_PROJECT_ID

#include <string>

#include "deadbeef/deadbeef.h"

#include "config.hpp"
#include "constants.hpp"
#include "log.hpp"

typedef struct {
    DB_misc_t plugin;
    ddb_ows::Configuration& conf;
    std::string (*get_output_path)(DB_playItem_t* it, char* format);
} ddb_ows_plugin_t;

#endif

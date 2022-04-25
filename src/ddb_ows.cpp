#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <string>
#include <errno.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/plugins/converter/converter.h>

#include "ddb_ows.hpp"
#include "plugin.hpp"


namespace ddb_ows{

class OWSPlugin : public Plugin{

};

extern "C" DB_plugin_t* ddb_ows_load(DB_functions_t* api) {
    return OWSPlugin::load(api);
}

}

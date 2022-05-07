#include <deadbeef/deadbeef.h>
#include <deadbeef/plugins/converter/converter.h>

#include "ddb_ows.hpp"

namespace ddb_ows{

DB_plugin_t definition_;
const char* configDialog_ = "";
static DB_functions_t* ddb_api;

int start() {
    return 0;
}

int stop() {
    return 0;
}

int disconnect(){
    return 0;
}

int connect(){
    return 0;
}

int handleMessage(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2){
    return 0;
}

void init(DB_functions_t* api) {
    definition_.api_vmajor = 1;
    definition_.api_vminor = 8;
    definition_.version_major = DDB_OWS_VERSION_MAJOR;
    definition_.version_minor = DDB_OWS_VERSION_MINOR;
    definition_.type = DB_PLUGIN_MISC;
    definition_.id = DDB_OWS_PROJECT_ID;
    definition_.name = DDB_OWS_PROJECT_NAME;
    definition_.descr = DDB_OWS_PROJECT_DESC;
    definition_.copyright = DDB_OWS_LICENSE_TEXT;
    definition_.website = DDB_OWS_PROJECT_URL;
    definition_.start = start;
    definition_.stop = stop;
    definition_.connect = connect;
    definition_.disconnect = disconnect;
    definition_.message = handleMessage;
    definition_.configdialog = configDialog_;
}

DB_plugin_t* load(DB_functions_t* api) {
    ddb_api = api;
    init(api);
    return &definition_;
}

extern "C" DB_plugin_t* ddb_ows_load(DB_functions_t* api) {
    return load(api);
}

}

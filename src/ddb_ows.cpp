#include <deadbeef/deadbeef.h>
#include <deadbeef/plugins/converter/converter.h>

#include <nlohmann/json.hpp>

#include "ddb_ows.hpp"
#include "config.hpp"

namespace ddb_ows{

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

const char* configDialog_ = "";

Configuration conf = Configuration();

ddb_ows_plugin_t plugin = {
    .plugin = {
        .plugin = {
            .type = DB_PLUGIN_MISC,
            .api_vmajor = 1,
            .api_vminor = 8,
            .version_major = DDB_OWS_VERSION_MAJOR,
            .version_minor = DDB_OWS_VERSION_MINOR,
            .id = DDB_OWS_PROJECT_ID,
            .name = DDB_OWS_PROJECT_NAME,
            .descr = DDB_OWS_PROJECT_DESC,
            .copyright = DDB_OWS_LICENSE_TEXT,
            .website = DDB_OWS_PROJECT_URL,
            .start = start,
            .stop = stop,
            .connect = connect,
            .disconnect = disconnect,
            .message = handleMessage,
            .configdialog = configDialog_,
        },
    },
    .conf = conf,
};

void init(DB_functions_t* api) {
    plugin.conf.set_api(api);
}

DB_plugin_t* load(DB_functions_t* api) {
    ddb_api = api;
    init(api);
    return (DB_plugin_t*) &plugin;
}

extern "C" DB_plugin_t* ddb_ows_load(DB_functions_t* api) {
    return load(api);
}

}

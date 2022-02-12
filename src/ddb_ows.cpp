#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <string>
#include <errno.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <unistd.h>

#include <pthread.h>
#include <linux/limits.h>

#include <deadbeef/deadbeef.h>

#include "defs.hpp"
#include "ddb_ows.hpp"


namespace ddb_ows{

const char Plugin::configDialog_ [] = "";

DB_plugin_t Plugin::definition_;

DB_plugin_t* Plugin::load(DB_functions_t* api) {
    ddb_api = api;
    init(api);
    return &definition_;
}

void Plugin::init(DB_functions_t* api) {
    auto& p = definition_;
    p.api_vmajor = 1;
    p.api_vminor = 8;
    p.version_major = DDB_OWS_VERSION_MAJOR;
    p.version_minor = DDB_OWS_VERSION_MINOR;
    p.type = DB_PLUGIN_MISC;
    p.id = DDB_OWS_PROJECT_ID;
    p.name = DDB_OWS_PROJECT_NAME;
    p.descr = DDB_OWS_PROJECT_DESC;
    p.copyright = DDB_OWS_LICENSE_TEXT;
    p.website = DDB_OWS_PROJECT_URL;
    p.start = start;
    p.stop = stop;
    p.connect = connect;
    p.disconnect = disconnect;
    p.message = handleMessage;
    p.configdialog = configDialog_;
}

int Plugin::start() {
    return 0;
}

int Plugin::stop() {
    return 0;
}

int Plugin::disconnect(){
    return 0;
}

int Plugin::connect(){
    return 0;
}

int Plugin::handleMessage(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2){
    return 0;
}

extern "C" DB_plugin_t* ddb_ows_load(DB_functions_t* api) {
    return Plugin::load(api);
}

}

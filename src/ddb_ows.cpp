#include <deadbeef/deadbeef.h>
#include <deadbeef/plugins/converter/converter.h>

#include <optional>
#include <string>

#include "ddb_ows.hpp"
#include "config.hpp"

namespace ddb_ows{

static DB_functions_t* ddb_api;
Configuration conf = Configuration();

void escape(std::string& s) {
    for (auto i = s.begin(); i != s.end(); i++) {
        switch (*i) {
            case '/':
            case '\\':
            case ':':
            case '*':
            case '?':
            case '"':
            case '<':
            case '>':
            case '|':
                *i = '-';
        }
    }
}

std::string get_output_path(DB_playItem_t* it, char* format) {
    DB_playItem_t* copy = ddb_api->pl_item_alloc();
    //std::basic_regex escape("[/\\:*?\"<>|]");
    ddb_api->pl_lock();
    DB_metaInfo_t* meta = ddb_api->pl_get_metadata_head(it);
    while (meta != NULL) {
        std::string val(meta->value);
        escape(val);
        ddb_api->pl_add_meta(copy, meta->key, val.c_str());
        meta = meta->next;
    }
    ddb_api->pl_unlock();
    char out[PATH_MAX];
    ddb_tf_context_t ctx = {
        ._size = sizeof(ddb_tf_context_t),
        .flags = 0,
        .it = copy,
        .plt = NULL,
        //TODO change this?
        .idx = 0,
        .id = 0,
        .iter = PL_MAIN,
    };
    ddb_api->tf_eval(&ctx, format, out, sizeof(out));
    ddb_api->pl_item_unref(copy);
    return std::string(out);
}


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
    .get_output_path = get_output_path
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

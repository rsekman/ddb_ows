#ifndef DDB_OWS_CONFIG_H

#define DDB_OWS_CONFIG_H

#include <string>
#include <vector>
#include <map>

#include <nlohmann/json.hpp>
#include "deadbeef/deadbeef.h"

#include "log.hpp"

#define DDB_OWS_CONFIG_MAIN "ddb_ows.settings"
#define DDB_OWS_CONFIG_KEY_ROOT "root"
#define DDB_OWS_CONFIG_KEY_FN_FORMATS "fn_formats"
#define DDB_OWS_CONFIG_KEY_COVER_SYNC "cover_sync"
#define DDB_OWS_CONFIG_KEY_COVER_FNAME "cover_fname"
#define DDB_OWS_CONFIG_KEY_RM_UNREF "rm_unref"
#define DDB_OWS_CONFIG_KEY_CONV_FTS "conv_fts"
#define DDB_OWS_CONFIG_KEY_CONV_PRESET "conv_preset"
#define DDB_OWS_CONFIG_KEY_CONV_EXT "conv_ext"
#define DDB_OWS_CONFIG_KEY_CONV_WTS "conv_wts"

#define DDB_OWS_CONFIG_METHODS(name, type, key) \
    type get_ ## name () { \
        return get_typesafe<type>(key); \
    }; \
    bool set_ ## name (type val) { \
        conf[key] = val; \
        write_conf(); \
        return true; \
    };

using nlohmann::json;

namespace ddb_ows {

class Configuration {
    public:
        // plumbing
        Configuration();
        void set_api(DB_functions_t* api);
        bool update_conf();

        DDB_OWS_CONFIG_METHODS(root, std::string, DDB_OWS_CONFIG_KEY_ROOT)
        DDB_OWS_CONFIG_METHODS(fn_formats, std::vector<std::string>, DDB_OWS_CONFIG_KEY_FN_FORMATS)
        DDB_OWS_CONFIG_METHODS(cover_sync, bool, DDB_OWS_CONFIG_KEY_COVER_SYNC)
        DDB_OWS_CONFIG_METHODS(cover_fname, std::string, DDB_OWS_CONFIG_KEY_COVER_FNAME)
        DDB_OWS_CONFIG_METHODS(rm_unref, bool, DDB_OWS_CONFIG_KEY_RM_UNREF)
        #define dummytype std::unordered_map<std::string, bool>
        // can't use commas in macro arguments naively
        DDB_OWS_CONFIG_METHODS(conv_fts, dummytype, DDB_OWS_CONFIG_KEY_CONV_FTS)
        #undef dummytype
        DDB_OWS_CONFIG_METHODS(conv_preset, std::string, DDB_OWS_CONFIG_KEY_CONV_PRESET)
        DDB_OWS_CONFIG_METHODS(conv_ext, std::string, DDB_OWS_CONFIG_KEY_CONV_EXT)
        DDB_OWS_CONFIG_METHODS(conv_wts, int, DDB_OWS_CONFIG_KEY_CONV_WTS)

    private:
        DB_functions_t* ddb;
        json conf;
        json default_conf;
        bool write_conf();
        template<typename T> T get_typesafe(const char* key){
            try {
                return conf.at(key);
            } catch (json::exception& e) {
                DDB_OWS_ERR << "Configuration type error: " << key << "; " << e.what() << "\n";
            }
            return default_conf.at(key);
        }

};

}

#endif

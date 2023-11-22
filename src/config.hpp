#ifndef DDB_OWS_CONFIG_H

#define DDB_OWS_CONFIG_H

#include <string>
#include <vector>
#include <set>
#include <unordered_set>

#include "deadbeef/deadbeef.h"

#include "playlist_uuid.hpp"

#define DDB_OWS_CONFIG_MAIN "ddb_ows.settings"

#define DDB_OWS_CONFIG_METHODS(name, type) \
    type get_ ## name () { \
        return _conf.name; \
    }; \
    bool set_ ## name (type val) { \
        _conf.name = val; \
        write_conf(); \
        return true; \
    };

namespace ddb_ows {

typedef struct {
    std::string root;
    std::vector<std::string> fn_formats;
    std::unordered_set<plt_uuid> pl_selection;
    bool cover_sync;
    std::string cover_fname;
    bool sync_pls;
    bool rm_unref;
    std::set<std::string> conv_fts;
    std::string conv_preset;
    std::string conv_ext;
    int conv_wts;
} ddb_ows_config;

class Configuration {
    public:
        // plumbing
        Configuration();
        void set_api(DB_functions_t* api);
        bool update_conf();

        DDB_OWS_CONFIG_METHODS(root, std::string)
        DDB_OWS_CONFIG_METHODS(fn_formats, std::vector<std::string>)
        DDB_OWS_CONFIG_METHODS(pl_selection, std::unordered_set<plt_uuid>)
        DDB_OWS_CONFIG_METHODS(cover_sync, bool)
        DDB_OWS_CONFIG_METHODS(cover_fname, std::string)
        DDB_OWS_CONFIG_METHODS(sync_pls, bool)
        DDB_OWS_CONFIG_METHODS(rm_unref, bool)
        DDB_OWS_CONFIG_METHODS(conv_fts, std::set<std::string>)
        DDB_OWS_CONFIG_METHODS(conv_preset, std::string)
        DDB_OWS_CONFIG_METHODS(conv_ext, std::string)
        DDB_OWS_CONFIG_METHODS(conv_wts, int)

    private:
        DB_functions_t* ddb;
        ddb_ows_config _conf;
        bool _update_conf(const char* buf);
        bool write_conf();
};

}

#endif

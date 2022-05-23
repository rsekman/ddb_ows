#ifndef DDB_OWS_CONFIG_H

#define DDB_OWS_CONFIG_H

#include <string>
#include <vector>
#include <map>
#include "deadbeef/deadbeef.h"

#define DDB_OWS_CONFIG_MAIN "ddb_ows.settings"
#define DDB_OWS_CONFIG_KEY_ROOT "root"
#define DDB_OWS_CONFIG_KEY_COVER_SYNC "cover_sync"
#define DDB_OWS_CONFIG_KEY_COVER_FNAME "cover_fname"
#define DDB_OWS_CONFIG_KEY_COVER_FNAME_DEFAULT "cover.jpg"
#define DDB_OWS_CONFIG_KEY_FN_FORMATS "fn_formats"
#define DDB_OWS_CONFIG_KEY_FT_SEL "conv_fts"
#define DDB_OWS_CONFIG_KEY_CONV_PRESET "conv_preset"
#define DDB_OWS_CONFIG_KEY_WT "wts"

namespace ddb_ows {

class Configuration {
    public:
        void set_api(DB_functions_t* api);

        std::string get_root();
        bool set_root(std::string root);

        std::vector<std::string> get_fn_formats();
        bool set_fn_formats(std::vector<std::string> formats);

        std::string get_cover_fname();
        bool set_cover_fname(std::string root);

        bool get_cover_sync();
        bool set_cover_sync(bool sync);

        std::map<std::string, bool> get_ft_selection();
        bool set_ft_selection(std::map<std::string, bool> selection);

        std::string get_preset();
        bool set_preset(std::string root);

        int get_worker_threads();
        bool set_worker_threads(int threads);
    private:
        DB_functions_t* ddb_api;
};

}

#endif

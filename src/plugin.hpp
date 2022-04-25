#ifndef DDB_OWS_PLUGIN_HPP
#define DDB_OWS_PLUGIN_HPP

#include <deadbeef/deadbeef.h>

#include "ddb_ows.hpp"

namespace ddb_ows{

class Plugin{
    public:
        Plugin();
        static DB_plugin_t* load(DB_functions_t* api);
    private:
        static void init(DB_functions_t* api);
        static DB_functions_t* ddb_api;
        static int start();
        static int stop();
        static int connect();
        static int disconnect();
        static int handleMessage(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2);
        static DB_plugin_t definition_;
        static const char* configDialog_;
        static int id;
};

}

#endif

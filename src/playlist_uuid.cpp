#include "playlist_uuid.hpp"

namespace ddb_ows {

plt_uuid plt_get_uuid(ddb_playlist_t* plt, DB_functions_t* ddb) {
    char buf[37] ;
    uuid_t id;

    int exists = ddb->plt_get_meta(plt, DDB_OWS_PL_UUID_KEY, buf, 37);
    if (!exists || uuid_parse(buf, id) < 0) {
        uuid_generate(id);
        uuid_unparse(id, buf);
        ddb->plt_add_meta(plt, DDB_OWS_PL_UUID_KEY, buf);
        // mark playlist as modified so it is saved
        ddb->plt_modified(plt);
    }
    return buf;
}

}

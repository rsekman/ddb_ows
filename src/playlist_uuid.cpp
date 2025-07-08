#include "playlist_uuid.hpp"

namespace ddb_ows {

plt_uuid::plt_uuid(uuid_t id) { uuid_copy(uuid, id); }

bool plt_uuid::operator==(const plt_uuid& other) const {
    uuid_t out;
    other.get(out);
    return uuid_compare(out, uuid) == 0;
}

void plt_uuid::get(uuid_t out) const { uuid_copy(out, uuid); }

std::size_t plt_uuid::hash() const noexcept {
    return std::hash<std::string>{}(str());
}

std::string plt_uuid::str() const {
    char buf[UUID_STR_LEN];
    uuid_unparse(uuid, buf);
    return std::string(buf);
}

plt_uuid plt_get_uuid(ddb_playlist_t* plt, DB_functions_t* ddb) {
    char buf[UUID_STR_LEN];
    uuid_t id;

    int exists = ddb->plt_get_meta(plt, DDB_OWS_PL_UUID_KEY, buf, UUID_STR_LEN);
    if (!exists || uuid_parse(buf, id) < 0) {
        uuid_generate(id);
        uuid_unparse(id, buf);
        ddb->plt_add_meta(plt, DDB_OWS_PL_UUID_KEY, buf);
        // mark playlist as modified so it is saved
        ddb->plt_modified(plt);
    }
    return plt_uuid(id);
}

}  // namespace ddb_ows

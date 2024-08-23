#ifndef DDB_OWS_PL_UUID_HPP
#define DDB_OWS_PL_UUID_HPP

#include <deadbeef/deadbeef.h>
#include <uuid/uuid.h>

#include <string>

#include "constants.hpp"

#define DDB_OWS_PL_UUID_KEY DDB_OWS_PROJECT_ID "_uuid"

namespace ddb_ows {

// retrieve the uuid of *plt, if it exists
// if it does not, generate it and return it

class plt_uuid {
  public:
    plt_uuid() {}
    plt_uuid(uuid_t);
    std::string str() const;
    void get(uuid_t out) const;
    bool operator==(const plt_uuid& other) const;
    std::size_t hash() const noexcept;

  private:
    uuid_t uuid;
};

plt_uuid plt_get_uuid(ddb_playlist_t* plt, DB_functions_t* ddb);

}  // namespace ddb_ows

template <>
struct std::hash<ddb_ows::plt_uuid> {
    std::size_t operator()(ddb_ows::plt_uuid const& uuid) const noexcept {
        return uuid.hash();
    }
};

#endif

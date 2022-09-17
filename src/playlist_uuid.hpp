#ifndef DDB_OWS_PL_UUID_HPP
#define DDB_OWS_PL_UUID_HPP

#include <deadbeef/deadbeef.h>
#include <string>
#include <uuid/uuid.h>
#include "constants.hpp"
#include "log.hpp"

#define DDB_OWS_PL_UUID_KEY DDB_OWS_PROJECT_ID "_uuid"

namespace ddb_ows {

// retrieve the uuid of *plt, if it exists
// if it does not, generate it and return it

// TODO use a wrapper class around uuid_t for type safety
// this is written with std::string for memory management
// but we need extra code to put the wrapper class in stdlib containers

typedef std::string plt_uuid;
plt_uuid plt_get_uuid(ddb_playlist_t* plt, DB_functions_t* ddb);

}

#endif

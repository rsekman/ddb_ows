#ifndef _DDB_OWS_LOG
#define _DDB_OWS_LOG

#include <iostream>
#define DDB_OWS_LOG_NONE 0
#define DDB_OWS_LOG_ERR 1
#define DDB_OWS_LOG_WARN 2
#define DDB_OWS_LOG_DEBUG 3

#ifndef DDB_OWS_LOGLEVEL
#define DDB_OWS_LOGLEVEL DDB_OWS_LOG_DEBUG
#endif

#define DDB_OWS_ERR                           \
    if (DDB_OWS_LOGLEVEL < DDB_OWS_LOG_ERR) { \
    } else                                    \
        std::cerr << "[ddb_ows] [error] "
#define DDB_OWS_WARN                           \
    if (DDB_OWS_LOGLEVEL < DDB_OWS_LOG_WARN) { \
    } else                                     \
        std::cerr << "[ddb_ows] [warn] "
#define DDB_OWS_DEBUG                           \
    if (DDB_OWS_LOGLEVEL < DDB_OWS_LOG_DEBUG) { \
    } else                                      \
        std::cerr << "[ddb_ows] [debug] "

#endif

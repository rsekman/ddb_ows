#include "logger.hpp"

#include <spdlog/spdlog.h>

namespace ddb_ows {

#define DDB_OWS_STDIO_LOGGER_METHOD(x, y)      \
    bool StdioLogger::x(std::string message) { \
        spdlog::y(message);                    \
        return true;                           \
    }

DDB_OWS_STDIO_LOGGER_METHOD(verbose, debug);
DDB_OWS_STDIO_LOGGER_METHOD(log, info);
DDB_OWS_STDIO_LOGGER_METHOD(warn, warn);
DDB_OWS_STDIO_LOGGER_METHOD(err, error);

}  // namespace ddb_ows

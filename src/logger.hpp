#ifndef DDB_OWS_LOGGER_HPP
#define DDB_OWS_LOGGER_HPP

#include <string>
#include <fmt/core.h>

namespace ddb_ows{

enum loglevel_e {
    DDB_OWS_TBL_VERBOSE,
    DDB_OWS_TBL_LOG,
    DDB_OWS_TBL_WARN,
    DDB_OWS_TBL_ERR,
};

#define DDB_OWS_LOGGER_METHOD(x) \
        virtual bool x(std::string message) = 0; \
        template<typename ...T> \
        bool x(fmt::format_string<T...> fmt, T&&... args) { \
            return x(fmt::format(fmt, args...)); \
        }

class Logger {
    public:
        virtual ~Logger() {};
        DDB_OWS_LOGGER_METHOD(verbose)
        DDB_OWS_LOGGER_METHOD(log)
        DDB_OWS_LOGGER_METHOD(warn)
        DDB_OWS_LOGGER_METHOD(err)

        virtual void clear() = 0;

};

class StdioLogger : public Logger {
    public:
        ~StdioLogger() { };
        bool verbose(std::string message);
        bool log(std::string message);
        bool warn(std::string message);
        bool err(std::string message);
        void clear() {};
};

}

#endif

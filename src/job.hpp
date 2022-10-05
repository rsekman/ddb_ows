#ifndef DDB_OWS_JOB_HPP
#define DDB_OWS_JOB_HPP

#include <filesystem>
#include <string>
#include <deadbeef/deadbeef.h>
#include <deadbeef/converter.h>

#include "database.hpp"
#include "logger.hpp"

using namespace std::filesystem;

namespace ddb_ows {

class Job {
    public:
        Job(Logger& _logger, ddb_ows::Database* _db, path _from, path _to) :
            logger(_logger),
            db(_db),
            from(_from),
            to(_to)
        {} ;
        virtual bool run(bool dry=false) = 0;
        virtual void abort() = 0;
        virtual ~Job() {};
    protected:
        Logger& logger;
        ddb_ows::Database* db;
        const path from;
        const path to;
        db_entry_t make_entry();
        void register_job();
        void register_job(db_entry_t entry);
};

class CopyJob : public Job {
    public:
        CopyJob( Logger& logger, ddb_ows::Database* db, path from, path to );
        bool run(bool dry=false);
        void abort() {
        }
};

class MoveJob : public Job {
    public:
        MoveJob( Logger& logger, ddb_ows::Database* db, path from, path to );
        bool run(bool dry=false);
        void abort() {
        }
};

class ConvertJob : public Job {
    public:
        ConvertJob(
            Logger& logger,
            ddb_ows::Database* db,
            DB_functions_t* ddb,
            ddb_converter_settings_t settings,
            DB_playItem_t* it,
            path from,
            path to
        );
        ~ConvertJob();
        bool run(bool dry=false);
        void abort();
    private:
        DB_functions_t* ddb;
        ddb_converter_settings_t settings;
        DB_playItem_t* it;
        int pabort;
};

}

#endif

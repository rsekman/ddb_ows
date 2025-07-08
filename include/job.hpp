#ifndef DDB_OWS_JOB_HPP
#define DDB_OWS_JOB_HPP

// clang-format off: includes must be in this order
#include <deadbeef/deadbeef.h>
#include <deadbeef/converter.h>
// clang-format on

#include <filesystem>

#include "database.hpp"
#include "logger.hpp"

using namespace std::filesystem;

namespace ddb_ows {

class Job {
  public:
    Job(Logger& _logger,
        DatabaseHandle _db,
        sync_id_t _sync_id,
        path _from,
        path _to) :
        logger(_logger), db(_db), from(_from), to(_to), sync_id(_sync_id) {};
    virtual bool run(bool dry = false) = 0;
    virtual void abort() = 0;
    virtual ~Job() {};

  protected:
    Logger& logger;
    DatabaseHandle db;
    const path from;
    const path to;
    const sync_id_t sync_id;
    virtual void register_job() = 0;
};

class CopyJob : public Job {
  public:
    CopyJob(
        Logger& logger, DatabaseHandle db, sync_id_t sync_id, path from, path to
    );
    bool run(bool dry = false) override;
    void abort() override {}

  private:
    void register_job() override;
};

class MoveJob : public Job {
  public:
    MoveJob(
        Logger& logger,
        DatabaseHandle db,
        sync_id_t sync_id,
        path from,
        path to,
        path source,
        std::optional<std::string> converter_preset
    );
    bool run(bool dry = false) override;
    void abort() override {}

  private:
    path source;
    std::optional<std::string> converter_preset;
    void register_job() override;
};

class ConvertJob : public Job {
  public:
    ConvertJob(
        Logger& logger,
        DatabaseHandle db,
        DB_functions_t* ddb,
        ddb_converter_settings_t settings,
        DB_playItem_t* it,
        sync_id_t sync_id,
        path from,
        path to
    );
    ~ConvertJob();
    bool run(bool dry = false) override;
    void abort() override;

  private:
    DB_functions_t* ddb;
    ddb_converter_settings_t settings;
    DB_playItem_t* it;
    int pabort;

    void register_job() override;
};

class DeleteJob : public Job {
  public:
    DeleteJob(
        Logger& logger,
        DatabaseHandle db,
        sync_id_t sync_id,
        path from,
        path target
    );
    bool run(bool dry = false);
    void abort() {};

  private:
    void register_job();
};

}  // namespace ddb_ows

#endif

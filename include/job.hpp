#ifndef DDB_OWS_JOB_HPP
#define DDB_OWS_JOB_HPP

// clang-format off: includes must be in this order
#include <deadbeef/deadbeef.h>
#include <deadbeef/converter.h>
// clang-format on

#include <filesystem>

#include "database.hpp"
#include "logger.hpp"

namespace ddb_ows {

class Job {
  protected:
    using path = std::filesystem::path;

  public:
    Job(std::shared_ptr<Logger> _logger,
        DatabaseHandle _db,
        sync_id_t _sync_id,
        path _source,
        path _destination) :
        logger(_logger), db(_db), source(_source), destination(_destination), sync_id(_sync_id) {};
    virtual bool run(bool dry = false) = 0;
    virtual void abort() = 0;
    virtual ~Job() {};

  protected:
    std::shared_ptr<Logger> logger;
    DatabaseHandle db;
    const path source;
    const path destination;
    const sync_id_t sync_id;
    virtual void register_job() = 0;
};

class CopyJob : public Job {
  public:
    CopyJob(
        std::shared_ptr<Logger> logger,
        DatabaseHandle db,
        sync_id_t sync_id,
        path source,
        path destination
    );
    bool run(bool dry = false) override;
    void abort() override {}

  private:
    void register_job() override;
};

class MoveJob : public Job {
  public:
    MoveJob(
        std::shared_ptr<Logger> _logger,
        DatabaseHandle db,
        sync_id_t sync_id,
        path source,
        path old_destination,
        path destination,
        std::optional<std::string> converter_preset
    );
    bool run(bool dry = false) override;
    void abort() override {}

  private:
    path old_destination;
    std::optional<std::string> converter_preset;
    void register_job() override;
};

class ConvertJob : public Job {
  public:
    ConvertJob(
        std::shared_ptr<Logger> logger,
        DatabaseHandle db,
        DB_functions_t* ddb,
        ddb_converter_settings_t settings,
        DB_playItem_t* it,
        sync_id_t sync_id,
        path source,
        path destination
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
        std::shared_ptr<Logger> _logger,
        DatabaseHandle db,
        sync_id_t sync_id,
        path source,
        path destination
    );
    bool run(bool dry = false) override;
    void abort() override {};

  private:
    void register_job() override;
};

}  // namespace ddb_ows

#endif

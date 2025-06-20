#include <mutex>

namespace ddb_ows {

class CancellationToken {
  public:
    bool get();
    void cancel();

  private:
    bool cancelled = false;
    std::mutex m;
};

}  // namespace ddb_ows

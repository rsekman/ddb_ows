#include <cancellationtoken.hpp>
#include <mutex>

namespace ddb_ows {

bool CancellationToken::get() {
    std::lock_guard lock(m);
    return cancelled;
}

void CancellationToken::cancel() {
    std::lock_guard lock(m);
    cancelled = true;
};
}  // namespace ddb_ows

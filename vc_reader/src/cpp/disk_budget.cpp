// Shared budget registry for volume and hosted-object caches.
#include "disk_cache.hpp"

namespace vcr::zarr {
std::shared_ptr<DiskCache> diskBudget(const std::string& root, std::optional<std::int64_t> maxBytes)
{
#ifdef _WIN32
    // Windows cannot delete a file while a handle is open on it, so an index kept for the
    // whole process would pin <root>/.vc_reader_cache.sqlite3 and the root could never be
    // removed. Share an index only while a reader holds it; the last one closes it.
    static std::mutex mutex;
    static std::map<std::string, std::weak_ptr<DiskCache>> budgets;
    std::lock_guard lock(mutex);
    auto budget = budgets[root].lock();
    if (!budget) {
        budget = std::make_shared<DiskCache>(root, maxBytes.value_or(-1));
        budgets[root] = budget;
    } else if (maxBytes) {
        budget->set_max_bytes(*maxBytes);
    }
    return budget;
#else
    static std::mutex mutex;
    static std::map<std::string, std::shared_ptr<DiskCache>> budgets;
    static pid_t owner = getpid();
    std::lock_guard lock(mutex);
    if (owner != getpid()) {  // a forked child: the parent's indexes are not ours to use
        for (auto& [_, budget] : budgets)
            new std::shared_ptr<DiskCache>(std::move(budget));  // leaked, never touched
        budgets.clear();
        owner = getpid();
    }
    auto& budget = budgets[root];
    if (!budget)
        budget = std::make_shared<DiskCache>(root, maxBytes.value_or(-1));
    else if (maxBytes)
        budget->set_max_bytes(*maxBytes);
    return budget;
#endif
}

} // namespace vcr::zarr

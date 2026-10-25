module;

#include <cstddef>
#include <memory>
#include <vector>
#include <optional>
#include <stdexcept>
#include <utility>

export module multi_level_cache;
import abstract_cache;

export namespace caches {

enum class CacheTypes {
  ARC,
  LFU,
  LRU,
  LIRS,
  TW0_QUEUE,
};

template <typename Value, typename Key = int>
class multi_level_cache
{
  std::vector<std::unique_ptr<abstract_cache<Value, Key>>> levels_;

  public:

  explicit multi_level_cache(
    std::vector<std::unique_ptr<abstract_cache<Value, Key>>> levels)
    : levels_(std::move(levels))
  {
    std::size_t previous_capacity = 0;
    for (const auto& level : levels_)
    {
      if (!level || !level->capacity())
        throw std::invalid_argument("Cache levels must be non-null and have positive capacity");

      const std::size_t current_capacity = level->capacity();
      if (current_capacity < previous_capacity)
        throw std::invalid_argument("Cache level capacities must be nondecreasing from L1 downwards");

      previous_capacity = current_capacity;
    }
  }

  template <typename F> bool lookup_update(Key key, F slow_get_page)
  {
    if (levels_.empty()) return false;

    const std::size_t levels_count = levels_.size();
    std::size_t found = levels_count;
    std::optional<Value> value;

    for (std::size_t i = 0; i < levels_count; ++i)
    {
      if (const Value* stored = levels_[i]->peek(key))
      {
        if (i > 0) value.emplace(*stored);
        found = i;
        break;
      }
    }

    if (found == levels_count) value.emplace(slow_get_page(key));

    for (std::size_t i = found; i < levels_count; ++i)
      levels_[i]->touch(key);

    for (std::size_t cursor = found; cursor > 0; --cursor)
    {
      const std::size_t i = cursor - 1;
      auto victim = levels_[i]->insert(key, *value);

      if (victim)
      {
        for (std::size_t upper = 0; upper < i; ++upper)
          levels_[upper]->invalidate(*victim);
      }
    }

    return found != levels_count;
  }
};

}

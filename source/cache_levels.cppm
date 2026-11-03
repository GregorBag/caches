module;

#include <cstddef>
#include <memory>
#include <vector>
#include <stdexcept>
#include <utility>
#include <initializer_list>

export module cache_levels;

import abstract_cache;
import arc_cache;
import lfu_cache;
import lru_cache;
import lirs_cache;
import two_queue_cache;

export namespace caches {

enum class CacheTypes {
  ARC,
  LFU,
  LRU,
  LIRS,
  TWO_QUEUE,
};

namespace detail {

template <typename Value, typename Key>
std::vector<std::unique_ptr<abstract_cache<Value, Key>>> make_cache_levels(
  std::initializer_list<std::pair<CacheTypes, std::size_t>> configs)
{
  std::vector<std::unique_ptr<abstract_cache<Value, Key>>> levels;
  levels.reserve(configs.size());
  std::size_t previous_capacity = 0;

  for (const auto& [type, capacity] : configs)
  {
    if (!capacity)
      throw std::invalid_argument("Cache levels must have positive capacity");
    if (capacity < previous_capacity)
      throw std::invalid_argument("Cache level capacities must be nondecreasing from L1 downwards");
    previous_capacity = capacity;

    switch (type)
    {
      case CacheTypes::ARC:
        levels.push_back(
          std::make_unique<arc_cache<Value, Key>>(capacity));
        break;

      case CacheTypes::LFU:
        levels.push_back(
          std::make_unique<lfu_cache<Value, Key>>(capacity));
        break;

      case CacheTypes::LRU:
        levels.push_back(
          std::make_unique<lru_cache<Value, Key>>(capacity));
        break;

      case CacheTypes::LIRS:
        levels.push_back(
          std::make_unique<lirs_cache<Value, Key>>(capacity));
        break;

      case CacheTypes::TWO_QUEUE:
        levels.push_back(
          std::make_unique<two_queue_cache<Value, Key>>(capacity));
        break;

      default:
        throw std::invalid_argument("Unknown cache type");
    }
  }

  return levels;
}

} // namespace detail

} // namespace caches

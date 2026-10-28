module;

#include <cstddef>
#include <memory>
#include <vector>
#include <optional>
#include <stdexcept>
#include <utility>
#include <initializer_list>

export module multi_level_cache;

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

template <typename Value, typename Key = int>
class multi_level_cache : public abstract_cache<Value, Key>
{
  using Base = abstract_cache<Value, Key>;
  std::vector<std::unique_ptr<abstract_cache<Value, Key>>> levels_;

  std::optional<Key> insert_into_levels(std::size_t count, const Key& key, const Value& value)
  {
    std::optional<Key> victim_key;
    for (std::size_t cursor = count; cursor > 0; --cursor)
    {
      const std::size_t i = cursor - 1;
      auto victim = Base::insert_into(*levels_[i], key, value);

      if (cursor == levels_.size()) victim_key = victim;

      if (victim)
      {
        for (std::size_t upper = 0; upper < i; ++upper)
          levels_[upper]->invalidate(*victim);
      }
    }
    return victim_key;
  }

  std::optional<Key> insert(const Key& key, const Value& value) override
  {
    return insert_into_levels(levels_.size(), key, value);
  }

  using LevelConfig = std::pair<CacheTypes, std::size_t>;
  using Levels = std::vector<std::unique_ptr<Base>>;

  static Levels make_levels(std::initializer_list<LevelConfig> configs)
  {
    Levels levels;
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

  public:

  explicit multi_level_cache(std::initializer_list<LevelConfig> configs)
  : levels_(make_levels(configs)) {}

  bool full() const override
  {
    return levels_.empty() || levels_.back()->full();
  }

  std::size_t capacity() const override
  {
    return levels_.empty() ? 0 : levels_.back()->capacity();
  }

  const Value* peek(const Key& key) const override
  {
    for (const auto& level : levels_)
    {
      if (const Value* stored = level->peek(key)) return stored;
    }
    return nullptr;
  }

  void touch(const Key& key) override
  {
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

    if (found == levels_count) return;

    for (std::size_t i = found; i < levels_count; ++i)
      levels_[i]->touch(key);

    if (found > 0) insert_into_levels(found, key, *value);
  }

  void invalidate(const Key& key) override
  {
    for (auto& level : levels_) level->invalidate(key);
  }
};

}

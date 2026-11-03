module;

#include <cstddef>
#include <memory>
#include <vector>
#include <optional>
#include <utility>
#include <initializer_list>

export module multi_level_inclusive_cache;

import abstract_cache;
import arc_cache;
import lfu_cache;
import lru_cache;
import lirs_cache;
import two_queue_cache;
export import cache_levels;

export namespace caches {

template <typename Value, typename Key = int>
class multi_level_inclusive_cache final : public abstract_cache<Value, Key>
{
  using Base = abstract_cache<Value, Key>;
  using EvictedEntry = typename Base::EvictedEntry;
  std::vector<std::unique_ptr<abstract_cache<Value, Key>>> levels_;

  std::optional<EvictedEntry> insert_into_levels(std::size_t count, const Key& key, const Value& value)
  {
    std::optional<EvictedEntry> victim_entry;
    for (std::size_t cursor = count; cursor > 0; --cursor)
    {
      const std::size_t i = cursor - 1;
      auto victim = Base::insert_into(*levels_[i], key, value);

      if (cursor == levels_.size() && victim) victim_entry.emplace(*victim);

      if (victim)
      {
        for (std::size_t upper = 0; upper < i; ++upper)
          levels_[upper]->invalidate(victim->first);
      }
    }
    return victim_entry;
  }

  std::optional<EvictedEntry> insert(const Key& key, const Value& value) override
  {
    return insert_into_levels(levels_.size(), key, value);
  }

  using LevelConfig = std::pair<CacheTypes, std::size_t>;

  public:

  explicit multi_level_inclusive_cache(std::initializer_list<LevelConfig> configs)
  : levels_(detail::make_cache_levels<Value, Key>(configs)) {}

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

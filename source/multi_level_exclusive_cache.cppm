module;

#include <cstddef>
#include <memory>
#include <vector>
#include <optional>
#include <stdexcept>
#include <limits>
#include <utility>
#include <initializer_list>

export module multi_level_exclusive_cache;

import abstract_cache;
import arc_cache;
import lfu_cache;
import lru_cache;
import lirs_cache;
import two_queue_cache;
export import cache_levels;

export namespace caches {

template <typename Value, typename Key = int>
class multi_level_exclusive_cache final : public abstract_cache<Value, Key>
{
  using Base = abstract_cache<Value, Key>;
  using EvictedEntry = typename Base::EvictedEntry;
  std::vector<std::unique_ptr<Base>> levels_;
  std::size_t capacity_ = 0;

  std::optional<EvictedEntry> insert(const Key& key, const Value& value) override
  {
    if (levels_.empty()) return std::nullopt;

    auto victim = Base::insert_into(*levels_.front(), key, value);
    for (std::size_t i = 1; i < levels_.size() && victim; ++i)
    {
      auto next = Base::insert_into(*levels_[i], victim->first, victim->second);
      if (!next) return std::nullopt;
      victim.emplace(std::move(*next));
    }
    return victim;
  }

  using LevelConfig = std::pair<CacheTypes, std::size_t>;

  public:

  explicit multi_level_exclusive_cache(std::initializer_list<LevelConfig> configs)
  : levels_(detail::make_cache_levels<Value, Key>(configs))
  {
    for (const auto& level : levels_)
    {
      if (level->capacity() > std::numeric_limits<std::size_t>::max() - capacity_)
        throw std::invalid_argument("Total cache capacity is too large");
      capacity_ += level->capacity();
    }
  }

  bool full() const override
  {
    for (const auto& level : levels_)
    {
      if (!level->full()) return false;
    }
    return true;
  }

  std::size_t capacity() const override { return capacity_; }

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
    for (std::size_t i = 0; i < levels_.size(); ++i)
    {
      if (const Value* stored = levels_[i]->peek(key))
      {
        if (i == 0)
          levels_[i]->touch(key);
        else
        {
          Value value = *stored;
          levels_[i]->invalidate(key);
          insert(key, value);
        }
        return;
      }
    }
  }

  void invalidate(const Key& key) override
  {
    for (auto& level : levels_) level->invalidate(key);
  }
};

} // namespace caches

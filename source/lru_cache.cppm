module;

#include <cstddef>
#include <list>
#include <unordered_map>
#include <optional>

export module lru_cache;
import abstract_cache;

export namespace caches {

template <typename Value, typename Key = int>
class lru_cache final : public abstract_cache<Value, Key>
{
  using EvictedEntry = typename abstract_cache<Value, Key>::EvictedEntry;
  using ListIt = typename std::list<Key>::iterator;

  struct Entry {
    Value value;
    ListIt position;
  };

  std::size_t capacity_;
  std::list<Key> cache_;
  std::unordered_map<Key, Entry> entries_;

  public:
  
  explicit lru_cache(std::size_t capacity) : capacity_(capacity) {}
  bool full() const override { return (cache_.size() == capacity_); }
  std::size_t capacity() const override { return capacity_; }

  const Value* peek(const Key& key) const override
  {
    auto hit = entries_.find(key);
    if (hit != entries_.end()) return &hit->second.value;
    return nullptr;
  }

  void touch(const Key& key) override
  {
    auto hit = entries_.find(key);
    if (hit == entries_.end()) return;

    auto eltit = hit->second.position;
    cache_.splice(cache_.begin(), cache_, eltit);
  }

  private:

  std::optional<EvictedEntry> insert(const Key& key, const Value& value) override
  {
    if (!capacity_) return std::nullopt;
    std::optional<EvictedEntry> victim = std::nullopt;

    if (full()) {
      const Key& victim_key = cache_.back();
      victim.emplace(victim_key, entries_.at(victim_key).value);
      entries_.erase(victim_key);
      cache_.pop_back();
    }
    cache_.emplace_front(key);
    entries_.emplace(key, Entry{value, cache_.begin()});
    return victim;
  }

  public:

  void invalidate(const Key& key) override
  {
    auto hit = entries_.find(key);
    if (hit == entries_.end()) return;

    cache_.erase(hit->second.position);
    entries_.erase(hit);
  }
};

} // namespace caches

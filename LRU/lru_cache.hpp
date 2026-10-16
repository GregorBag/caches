#pragma once

#include <cstddef>
#include <list>
#include <unordered_map>
#include <optional>

namespace caches { 

template <typename T, typename KeyT = int> class lru_cache_t {
  using ListIt = typename std::list<KeyT>::iterator;

  struct Entry {
    T value;
    ListIt position;
  };

  std::size_t capacity_;
  std::list<KeyT> cache_;
  std::unordered_map<KeyT, Entry> entries_;

  bool full() const { return (cache_.size() == capacity_); }

  std::optional<KeyT> insert(const KeyT& key, const T& value)
  {
    if (!capacity_) return std::nullopt;
    std::optional<KeyT> victim_key = std::nullopt;

    if (full()) {
      victim_key = cache_.back();
      entries_.erase(*victim_key);
      cache_.pop_back();
    }
    cache_.emplace_front(key);
    entries_.emplace(key, Entry{value, cache_.begin()});
    return victim_key;
  }

  void invalidate(const KeyT& key)
  {
    auto hit = entries_.find(key);
    if (hit == entries_.end()) return;

    cache_.erase(hit->second.position);
    entries_.erase(hit);
  }

  public:
  
  explicit lru_cache_t(std::size_t capacity) : capacity_(capacity) {}

  template <typename F> bool lookup_update(KeyT key, F slow_get_page)
  {
    if (!capacity_) return false;
    if (peek(key))
    {
      touch(key);
      return true;
    }
    insert(key, slow_get_page(key));
    return false;
  }

  const T* peek(const KeyT& key) const
  {
    auto hit = entries_.find(key);
    if (hit != entries_.end()) return &hit->second.value;
    return nullptr;
  }

  void touch(const KeyT& key)
  {
    auto hit = entries_.find(key);
    if (hit == entries_.end()) return;

    auto eltit = hit->second.position;
    cache_.splice(cache_.begin(), cache_, eltit);
  }
};

} // namespace caches

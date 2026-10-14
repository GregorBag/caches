#pragma once

#include <cstddef>
#include <list>
#include <unordered_map>
#include <optional>

namespace caches {

template <typename T, typename KeyT = int> class lfu_cache_t { 
  const std::size_t capacity_;
  std::size_t min_freq_ = 0;

  using ListIt = typename std::list<KeyT>::iterator;

  struct Entry {
    T value;
    std::size_t freq;
    ListIt position;
  };

  std::unordered_map<KeyT, Entry> entries_;
  std::unordered_map<std::size_t, std::list<KeyT>> groups_;
  
  bool full() const { return (entries_.size() == capacity_); }

  std::optional<KeyT> insert(const KeyT& key, const T& value)
  {
    if (!capacity_) return std::nullopt;
    std::optional<KeyT> victim_key = std::nullopt;

    if (full()) {
      auto& min_group = groups_.at(min_freq_);
      victim_key = min_group.back();

      entries_.erase(*victim_key);
      min_group.pop_back();

      if (min_group.empty()) groups_.erase(min_freq_);
    }

    auto& group_freq_1 = groups_[1];
    group_freq_1.emplace_front(key);

    entries_.emplace(key, Entry{value, 1, group_freq_1.begin()});

    min_freq_ = 1;
    return victim_key;
  }

  void invalidate(const KeyT& key)
  {
    auto hit = entries_.find(key);
    if (hit == entries_.end()) return;

    auto& entry = hit->second;
    auto freq = entry.freq;
    auto& group = groups_.at(freq);

    group.erase(entry.position);
    entries_.erase(key);

    if (group.empty()) 
    {
      groups_.erase(freq);
      if (entries_.empty()) 
      {
        min_freq_ = 0;
      } else if (freq == min_freq_) 
      {
        min_freq_ = groups_.begin()->first;
        for (const auto& item : groups_) 
        {
          if (item.first < min_freq_) min_freq_ = item.first;
        }
      }
    }
  }

  public:

  explicit lfu_cache_t(std::size_t capacity): capacity_(capacity) {}

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

    auto& entry = hit->second;

    const auto old_freq = entry.freq;
    auto& old_group = groups_.at(old_freq);
    auto& new_group = groups_[old_freq + 1];

    new_group.splice(new_group.begin(), old_group, entry.position);

    entry.freq++;

    if (old_group.empty()) 
    {
      groups_.erase(old_freq);

      if (min_freq_ == old_freq) min_freq_++;
    }
  }

};

} // namespace caches
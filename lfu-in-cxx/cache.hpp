#pragma once

#include <cstddef>
#include <list>
#include <unordered_map>

namespace caches {

template <typename T, typename KeyT = int> struct cache_t { 
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

  explicit cache_t(std::size_t capacity): capacity_(capacity) {}

  //end of data structures

  bool full() const { return (entries_.size() == capacity_); }

  template <typename F> bool lookup_update(KeyT key, F slow_get_page) {
    if (!capacity_) return false;

    auto hit = entries_.find(key);
    if (hit != entries_.end()) {
      auto& entry = hit->second;

      const auto old_freq = entry.freq;
      auto& old_group = groups_.at(old_freq);
      auto& new_group = groups_[old_freq + 1];

      new_group.splice(new_group.begin(), old_group, entry.position);

      entry.freq++;

      if (old_group.empty()) {
        groups_.erase(old_freq);

        if (min_freq_ == old_freq) min_freq_++;
      }

      return true;
    }

    T value = slow_get_page(key);

    if (full()) {
      auto& min_group = groups_.at(min_freq_);
      KeyT delete_key = min_group.back();

      entries_.erase(delete_key);
      min_group.pop_back();

      if (min_group.empty()) groups_.erase(min_freq_);
    }

    auto& group_freq_1 = groups_[1];
    group_freq_1.emplace_front(key);

    entries_.emplace(key, Entry{value, 1, group_freq_1.begin()});

    min_freq_ = 1;

    return false;
  }

};

} // namespace caches
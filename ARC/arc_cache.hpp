#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <optional>

namespace caches {

template <typename T, typename KeyT = int> class arc_cache_t {
  using ListIt = typename std::list<KeyT>::iterator;

  enum class ResidentQueue { T1, T2 };

  enum class HistoryQueue { B1, B2 };

  struct ResidentEntry {
    T value;
    ResidentQueue queue;
    ListIt position;
  };

  struct HistoryEntry {
    HistoryQueue queue;
    ListIt position;
  };

  std::list<KeyT> T1_;
  std::list<KeyT> T2_;
  std::list<KeyT> B1_;
  std::list<KeyT> B2_;

  std::unordered_map<KeyT, ResidentEntry> entries_;
  std::unordered_map<KeyT, HistoryEntry> history_;

  const std::size_t capacity_;
  std::size_t p_ = 0;

  bool full() const { return (entries_.size() == capacity_); }

  size_t total_size() { return entries_.size() + history_.size(); }

  const KeyT replace(bool x_in_B2) 
  {
    if (!T1_.empty() && (T1_.size() > p_ || (x_in_B2 && T1_.size() == p_))) 
    {
      KeyT victim_key = T1_.back();
      entries_.erase(victim_key);

      B1_.splice(B1_.begin(), T1_, --T1_.end());
      history_.emplace(victim_key, HistoryEntry{HistoryQueue::B1, B1_.begin()});
      return victim_key;
    }
    KeyT victim_key = T2_.back();
    entries_.erase(victim_key);

    B2_.splice(B2_.begin(), T2_, --T2_.end());
    history_.emplace(victim_key, HistoryEntry{HistoryQueue::B2, B2_.begin()});
    return victim_key;
  }

  template <typename Entry>
  static void erase_back_block(std::list<KeyT>& list, std::unordered_map<KeyT, Entry>& table) 
  {
    table.erase(list.back());
    list.pop_back();
  }

  void p_increase() 
  {
    auto delta = std::max<std::size_t>(1, B2_.size()/B1_.size());
    p_ = std::min(capacity_, p_ + delta);
  }

  void p_decrease()
  {
    auto delta = std::max<std::size_t>(1, B1_.size() / B2_.size());
    p_ = delta >= p_ ? 0 : p_ - delta;
  }

  std::optional<KeyT> insert(const KeyT& key, const T& value)
  {
    if (!capacity_) return std::nullopt;
    std::optional<KeyT> victim_key = std::nullopt;
    auto history_hit = history_.find(key);

    if (history_hit != history_.end()) {
      auto& history_entry = history_hit->second;

      if (history_entry.queue == HistoryQueue::B1) {
        p_increase();

        T2_.splice(T2_.begin(), B1_, history_entry.position);
        history_.erase(key);

        if (full()) victim_key = replace(false);

        entries_.emplace(key, ResidentEntry{value, ResidentQueue::T2, T2_.begin()});
        return victim_key;
      }
      p_decrease();
      
      T2_.splice(T2_.begin(), B2_, history_entry.position);
      history_.erase(key);

      if (full()) victim_key = replace(true);

      entries_.emplace(key, ResidentEntry{value, ResidentQueue::T2, T2_.begin()});
      return victim_key;
    }

    if (T1_.size() + B1_.size() >= capacity_) {
      if (T1_.size() < capacity_) {
        erase_back_block(B1_, history_);
        if (full()) victim_key = replace(false);
      } else {
        victim_key = T1_.back();
        erase_back_block(T1_, entries_);
      }

    } else {
      if (total_size() >= capacity_) {
        if (total_size() == 2 * capacity_) {
          erase_back_block(B2_, history_);
        }
        if (full()) victim_key = replace(false);
      }
    }

    T1_.push_front(key);
    entries_.emplace(key, ResidentEntry{value, ResidentQueue::T1, T1_.begin()});
    return victim_key;
  }

  void invalidate(const KeyT& key)
  {
    auto hit = entries_.find(key);
    if (hit == entries_.end()) return;

    auto& entry = hit->second;
    if (entry.queue == ResidentQueue::T1)
      T1_.erase(entry.position);
    else
      T2_.erase(entry.position);

    entries_.erase(hit);
  }


  public:

  explicit arc_cache_t(std::size_t capacity): capacity_(capacity) {}

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
    if (hit != entries_.end()) {
      auto& entry = hit->second;

      if (entry.queue == ResidentQueue::T1) {
        entry.queue = ResidentQueue::T2;
        T2_.splice(T2_.begin(), T1_, entry.position);
        return;
      }

      T2_.splice(T2_.begin(), T2_, entry.position);
    }
  }

};

} // namespace caches
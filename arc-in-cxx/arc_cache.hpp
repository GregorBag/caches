#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <list>
#include <unordered_map>

namespace caches {

template <typename T, typename KeyT = int> struct cache_t {
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

  //

  std::list<KeyT> T1_;
  std::list<KeyT> T2_;
  std::list<KeyT> B1_;
  std::list<KeyT> B2_;

  std::unordered_map<KeyT, ResidentEntry> entries_;
  std::unordered_map<KeyT, HistoryEntry> history_;

  const std::size_t capacity_;
  std::size_t p_ = 0;

  explicit cache_t(std::size_t capacity): capacity_(capacity) {}

  //end of data structures

  bool full() const { return (entries_.size() == capacity_); }

  size_t total_size() { return entries_.size() + history_.size(); }

  void occupancy_solution(bool x_in_B2) 
  {
    if (!T1_.empty() && (T1_.size() > p_ || (x_in_B2 && T1_.size() == p_))) {
      B1_.splice(B1_.begin(), T1_, entries_.find(T1_.back())->second.position); //лажа
      history_.emplace(B1_.front(), HistoryEntry{HistoryQueue::B1, B1_.begin()});
      entries_.erase(B1_.front());

    } else {
      B2_.splice(B2_.begin(), T2_, entries_.find(T2_.back())->second.position); //лажа
      history_.emplace(B2_.front(), HistoryEntry{HistoryQueue::B2, B2_.begin()});
      entries_.erase(B2_.front());
    }
  }

  template <typename F> bool lookup_update(KeyT key, F slow_get_page) {
    if (!capacity_) return false;

    auto hit = entries_.find(key);

    if (hit != entries_.end()) {
      auto& entry = hit->second;

      if (entry.queue == ResidentQueue::T1) {
        entry.queue = ResidentQueue::T2;
        T2_.splice(T2_.begin(), T1_, entry.position);
        return true;
      }

      T2_.splice(T2_.begin(), T2_, entry.position);
      return true;
    }

    T value = slow_get_page(key);
    auto history_hit = history_.find(key);

    if (history_hit != history_.end()) {
      auto& history_entry = history_hit->second;

      if (history_entry.queue == HistoryQueue::B1) {
        p_ = std::min(capacity_, p_ + std::max<std::size_t>(1, B2_.size()/B1_.size())); //delta
        T2_.splice(T2_.begin(), B1_, history_entry.position);
        history_.erase(key);

        occupancy_solution(false);

        entries_.emplace(key, ResidentEntry{value, ResidentQueue::T2, T2_.begin()});
        return false;
      }

      auto delta = std::max<std::size_t>(1, B1_.size() / B2_.size());
      p_ = delta >= p_ ? 0 : p_ - delta;
      T2_.splice(T2_.begin(), B2_, history_entry.position);
      history_.erase(key);

      occupancy_solution(true);

      entries_.emplace(key, ResidentEntry{value, ResidentQueue::T2, T2_.begin()});
      return false;
    }

    if (T1_.size() + B1_.size() >= capacity_) {
      if (T1_.size() < capacity_) {
        history_.erase(B1_.back());
        B1_.pop_back();

        occupancy_solution(false);
      } else {
        entries_.erase(T1_.back());
        T1_.pop_back();
      }

    } else {
      if (total_size() >= capacity_) {
        if (total_size() == 2*capacity_) {
          history_.erase(B2_.back());
          B2_.pop_back();
        }

        occupancy_solution(false);
      }
    }

    T1_.push_front(key);
    entries_.emplace(key, ResidentEntry{value, ResidentQueue::T1, T1_.begin()});
    return false;
  }

};

} // namespace caches
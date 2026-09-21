#pragma once

#include <cmath>
#include <cstddef>
#include <list>
#include <unordered_map>

namespace caches {

template <typename T, typename KeyT = int> struct cache_t {
  using ListIt = typename std::list<KeyT>::iterator;

  enum class Queue {
    A1in,
    Am
  };

  struct Entry {
    T value;
    Queue queue;
    ListIt position;
  };

  std::list<KeyT> a1in_;
  std::list<KeyT> a1out_;
  std::list<KeyT> am_;
  
  std::unordered_map<KeyT, Entry> entries_;
  std::unordered_map<KeyT, ListIt> history_;
  
  std::size_t capacity_;

  explicit cache_t(std::size_t capacity): capacity_(capacity) {}

  std::size_t capacity_Ain_ = capacity_ / 4;
  std::size_t capacity_Aout_ = capacity_ / 2;

  //end of data structures

  bool full() const { return (entries_.size() == capacity_); }

  void occupancy_solution() {
    if (full() && a1in_.size() > capacity_Ain_) {
      KeyT excess_key = a1in_.back();
      ListIt excess_it = entries_.find(excess_key)->second.position;

      entries_.erase(excess_key);
      a1out_.splice(a1out_.begin(), a1in_, excess_it);
      history_.emplace(excess_key, excess_it); 
      //before if {} because capacity_Aout_ can be 0

      if (a1out_.size() > capacity_Aout_) {
        history_.erase(a1out_.back());
        a1out_.pop_back();
      }
    } 

    else if (full()) {
      entries_.erase(am_.back());
      am_.pop_back();
    }
  }

  template <typename F> bool lookup_update(KeyT key, F slow_get_page) {
    if (!capacity_) return false;

    auto hit = entries_.find(key);
    if (hit != entries_.end()) {
      auto& entry = hit->second;

      if (entry.queue == Queue::Am) {
        am_.splice(am_.begin(), am_, entry.position);
      }
      //if hit in A1in we do nothing
      return true;
    }

    T value = slow_get_page(key);
    auto history_hit = history_.find(key);

    if (history_hit != history_.end()) {
      am_.splice(am_.begin(), a1out_, history_hit->second);
      history_.erase(key);
      
      occupancy_solution();
      
      entries_.emplace(key, Entry{value, Queue::Am, am_.begin()});
      return false;
    }

    occupancy_solution();
    a1in_.emplace_front(key);
    entries_.emplace(key, Entry{value, Queue::A1in, a1in_.begin()});
    return false;
  }

};

} // namespace caches
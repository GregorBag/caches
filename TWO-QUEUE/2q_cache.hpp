#pragma once

#include <cmath>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <optional>

namespace caches {

template <typename T, typename KeyT = int> class two_queue_cache_t {
  using ListIt = typename std::list<KeyT>::iterator;

  enum class ResidentQueue {
    A1in,
    Am
  };

  struct Entry {
    T value;
    ResidentQueue queue;
    ListIt position;
  };

  std::list<KeyT> a1in_;
  std::list<KeyT> a1out_;
  std::list<KeyT> am_;
  
  std::unordered_map<KeyT, Entry> entries_;
  std::unordered_map<KeyT, ListIt> history_;
  
  const std::size_t capacity_;
  const std::size_t capacity_Ain_ = capacity_ / 4;
  const std::size_t capacity_Aout_ = capacity_ / 2;

  bool full() const { return (entries_.size() == capacity_); }

  const KeyT occupancy_solution() {
    if (a1in_.size() > capacity_Ain_) {
      KeyT victim_key = a1in_.back();
      ListIt victim_it = entries_.find(victim_key)->second.position;

      entries_.erase(victim_key);
      a1out_.splice(a1out_.begin(), a1in_, victim_it);
      history_.emplace(victim_key, victim_it); 

      if (a1out_.size() > capacity_Aout_) {
        history_.erase(a1out_.back());
        a1out_.pop_back();
      }
      return victim_key;
    } 

    KeyT victim_key = am_.back();
    entries_.erase(am_.back());
    am_.pop_back();
    return victim_key;
  }

  std::optional<KeyT> insert(const KeyT& key, const T& value)
  {
    if (!capacity_) return std::nullopt;
    std::optional<KeyT> victim_key = std::nullopt;
    auto history_hit = history_.find(key);

    if (history_hit != history_.end()) 
    {
      am_.splice(am_.begin(), a1out_, history_hit->second);
      history_.erase(key);
      
      if (full()) victim_key = occupancy_solution();
      
      entries_.emplace(key, Entry{value, ResidentQueue::Am, am_.begin()});
      return victim_key;
    }

    if (full()) victim_key = occupancy_solution();
    a1in_.emplace_front(key);
    entries_.emplace(key, Entry{value, ResidentQueue::A1in, a1in_.begin()});
    return victim_key;
  }

  void invalidate(const KeyT& key)
  {
    auto hit = entries_.find(key);
    if (hit == entries_.end()) return;

    auto& entry = hit->second;
    if (entry.queue == ResidentQueue::A1in)
      a1in_.erase(entry.position);
    else
      am_.erase(entry.position);

    entries_.erase(hit);
  }

  public:

  explicit two_queue_cache_t(std::size_t capacity): capacity_(capacity) {}

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
    if (entry.queue == ResidentQueue::Am) {
      am_.splice(am_.begin(), am_, entry.position);
    }
    //if hit in A1in we do nothing
  }

};

} // namespace caches
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <list>
#include <unordered_map>

namespace caches {

template <typename T, typename KeyT = int> struct cache_t {
  using ListIt = typename std::list<KeyT>::iterator;

  const std::size_t capacity_;
  const std::size_t lir_capacity_;
  const std::size_t hir_capacity_;
  std::size_t lir_size_ = 0;
  std::size_t hir_size_ = 0;

  enum class Kind { LIR, HIR };

  struct SEntry {
    T value;
    Kind kind;
    bool is_resident;
    ListIt position;
  };

  struct QEntry {
    T value;
    ListIt s_position;
  };

  std::list<KeyT> S_;  
  std::list<KeyT> Q_;

  std::unordered_map<KeyT, SEntry> S_map_; 
  std::unordered_map<KeyT, QEntry> Q_map_;

  explicit cache_t(std::size_t capacity): 
    capacity_(capacity), 
    hir_capacity_(std::max<std::size_t>(1, capacity / 100)),
    lir_capacity_(capacity_ - hir_capacity_) {}
    
  //end of data structures

  bool full() const {return ((lir_size_ + hir_size_) == capacity_);}

  void prune_stack() {}

  template <typename F> bool lookup_update(KeyT key, F slow_get_page) {
    if (!capacity_) return false;

    auto s_hit = S_map_.find(key);

    if (s_hit != S_map_.end()) 
    {
      auto& entry = s_hit->second;
      if (entry.kind == Kind::LIR) 
      {
        //hit LIR
        S_.splice(S_.begin(), S_, entry.position);

        prune_stack();
        return true;
      }

      
      if (entry.is_resident) 
      {
        //hit ResHIR in S
        S_.splice(S_.begin(), S_, entry.position);
        entry.kind = Kind::LIR;

        auto out_hit = Q_map_.find(key);
        Q_.erase(out_hit->second.position);
        Q_map_.erase(key);

        lir_size_++;
        hir_size_--;

        return true;
      }

      //hit NonResHIR in S

    }

    // auto q_hit = Q_map_.find(key);
    // //hit HIR
    // if (q_hit != Q_map_.end()) {
    //   auto& entry = q_hit->second;
      
    // }

  }

};

}  // namespace caches
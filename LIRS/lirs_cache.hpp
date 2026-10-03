#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <memory>
#include <utility>

namespace caches {

template <typename T, typename KeyT = int> class lirs_cache_t 
{
  using ListIt = typename std::list<KeyT>::iterator;

  const std::size_t capacity_;
  const std::size_t hir_capacity_;
  const std::size_t lir_capacity_;
  
  std::size_t lir_size_ = 0;
  std::size_t hir_size_ = 0;

  enum class Kind { LIR, HIR };

  struct SEntry 
  {
    std::unique_ptr<T> value;
    Kind kind;
    bool is_resident;
    ListIt position;
  };

  struct QEntry 
  {
    std::unique_ptr<T> value;
    ListIt position;
  };

  std::list<KeyT> S_;  
  std::list<KeyT> Q_;

  std::unordered_map<KeyT, SEntry> S_map_; 
  std::unordered_map<KeyT, QEntry> Q_map_;

  bool full() const { return ((lir_size_ + hir_size_) == capacity_); }

  void prune_stack() 
  {
    while (!S_.empty() && S_map_.find(S_.back())->second.kind == Kind::HIR) 
    {
      S_map_.erase(S_.back());
      S_.pop_back();
    }
  }

  void back_lir_retirement() 
  {
    KeyT victim_key = S_.back();
    auto& victim_entry = S_map_.find(victim_key)->second;

    Q_.emplace_back(victim_key);
    Q_map_.emplace(victim_key, QEntry {std::move(victim_entry.value), --Q_.end()});
    
    S_map_.erase(victim_key);
    S_.pop_back();

    hir_size_++;
    lir_size_--;

    prune_stack();
  }

  void front_q_retirement() 
  {
    KeyT victim_key = Q_.front();

    auto s_hit = S_map_.find(victim_key);
    if (s_hit != S_map_.end()) s_hit->second.is_resident = false;

    Q_map_.erase(victim_key);
    Q_.pop_front();

    hir_size_--;
  }


  public:

    explicit lirs_cache_t(std::size_t capacity): 
      capacity_(capacity), 
      hir_capacity_(capacity == 0 ? 0 : std::max<std::size_t>(1, capacity / 100)),
      lir_capacity_(capacity_ - hir_capacity_) {}


    template <typename F> bool lookup_update(KeyT key, F slow_get_page) 
    {
      if (!capacity_) return false;

      if (capacity_ == 1) 
      {
        //single value save in Q_map_
        if (Q_map_.find(key) != Q_map_.end()) return true;

        if (hir_size_ == 1) 
        { 
          Q_map_.clear(); 
          hir_size_--;
        }
        Q_map_.emplace(key, QEntry{std::make_unique<T>(slow_get_page(key)), Q_.end()});
        hir_size_++;
        return false;
      }

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
          auto out_hit = Q_map_.find(key);
          S_.splice(S_.begin(), S_, entry.position);
          entry.kind = Kind::LIR;
          entry.value = std::move(out_hit->second.value);
          
          Q_.erase(out_hit->second.position);
          Q_map_.erase(key);

          lir_size_++;
          hir_size_--;

          back_lir_retirement();

          return true;
        }

        //hit NonResHIR in S
        if (full()) front_q_retirement();
        entry.value = std::make_unique<T>(slow_get_page(key));
        entry.kind = Kind::LIR;
        entry.is_resident = true;
        lir_size_++;

        S_.splice(S_.begin(), S_, entry.position);
        back_lir_retirement();
        
        return false;
      }

      auto q_hit = Q_map_.find(key);

      if(q_hit != Q_map_.end()) 
      {
        //hit ResHir only in Q
        auto& entry = q_hit->second;
        Q_.splice(Q_.end(), Q_, entry.position);
        S_.push_front(key);
        S_map_.emplace(key, SEntry{nullptr, Kind::HIR, true, S_.begin()});

        return true;
      }

      //unknown hit
      if (full()) front_q_retirement();

      if (lir_size_ == lir_capacity_) 
      {
        Q_.push_back(key);
        Q_map_.emplace(key, QEntry{std::make_unique<T>(slow_get_page(key)), --Q_.end()});

        S_.push_front(key);
        S_map_.emplace(key, SEntry{nullptr, Kind::HIR, true, S_.begin()});

        hir_size_++;
      } else {
        S_.push_front(key);
        S_map_.emplace(key, SEntry{std::make_unique<T>(slow_get_page(key)), Kind::LIR, true, S_.begin()});

        lir_size_++;
      }
      
      return false;
    }

};

}  // namespace caches
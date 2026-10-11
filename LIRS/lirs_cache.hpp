#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <memory>
#include <utility>
#include <optional>

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

  std::optional<KeyT> evict_if_full()
  {
    if (!full()) return std::nullopt;
    return front_Q_retirement();
  }

  void queue_push_back(const KeyT& key, std::unique_ptr<T> value)
  {
    auto position = Q_.insert(Q_.end(), key);
    Q_map_.emplace(key, QEntry{std::move(value), position});
    ++hir_size_;
  }

  void promote_to_lir(SEntry& entry, std::unique_ptr<T> value)
  {
    entry.value = std::move(value);
    entry.kind = Kind::LIR;
    entry.is_resident = true;
    ++lir_size_;
    S_.splice(S_.begin(), S_, entry.position);
  }

  std::optional<KeyT> single_capacity_insert(const KeyT& key, const T& value) 
  {
    std::optional<KeyT> deleted_key = std::nullopt;
    if (hir_size_ == 1) 
    { 
      deleted_key = Q_.front();
      Q_.pop_front(); 
      Q_map_.clear(); hir_size_--; 
    }
    
    queue_push_back(key, std::make_unique<T>(value));
    return deleted_key;
  }

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

    queue_push_back(victim_key, std::move(victim_entry.value));
    S_map_.erase(victim_key);
    S_.pop_back();
    lir_size_--;

    prune_stack();
  }

  KeyT front_Q_retirement() 
  {
    KeyT victim_key = Q_.front();

    auto s_hit = S_map_.find(victim_key);
    if (s_hit != S_map_.end()) s_hit->second.is_resident = false;

    Q_map_.erase(victim_key);
    Q_.pop_front();

    hir_size_--;
    return victim_key;
  }

  void Q_erase(ListIt it)
  {
    Q_map_.erase(*it);
    Q_.erase(it);
  }

  std::optional<KeyT> unknown_hit_insert (const KeyT& key, const T& value) 
  {
    std::optional<KeyT> deleted_key = evict_if_full();

    if (lir_size_ == lir_capacity_) 
    {
      queue_push_back(key, std::make_unique<T>(value));
      S_.push_front(key);
      S_map_.emplace(key, SEntry{nullptr, Kind::HIR, true, S_.begin()});

      return deleted_key;
    } 

    S_.push_front(key);
    S_map_.emplace(key, SEntry{std::make_unique<T>(value), Kind::LIR, true, S_.begin()});

    lir_size_++;
    return deleted_key;
  }
  
  std::optional<KeyT> insert(const KeyT& key, const T& value)
  {
    std::optional<KeyT> deleted_key = std::nullopt;
    if (!capacity_) return deleted_key;
    if (capacity_ == 1) return single_capacity_insert(key, value);

    auto s_hit = S_map_.find(key);
    if (s_hit != S_map_.end())
    {
      //hit NonResHir in S
      auto& entry = s_hit->second;
      auto deleted_key_key = evict_if_full();
      promote_to_lir(entry, std::make_unique<T>(value));
      if (lir_size_ >= lir_capacity_) back_lir_retirement();
      
      return deleted_key;
    }

    //unknown hit
    return unknown_hit_insert(key, value);
  }

  public:

  explicit lirs_cache_t(std::size_t capacity): 
    capacity_(capacity), 
    hir_capacity_(capacity == 0 ? 0 : std::max<std::size_t>(1, capacity / 100)),
    lir_capacity_(capacity_ - hir_capacity_) {}


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
    auto s_hit = S_map_.find(key);
    if (s_hit != S_map_.end() && s_hit->second.kind == Kind::LIR) 
      return s_hit->second.value.get();

    auto q_hit = Q_map_.find(key);
    if (q_hit != Q_map_.end()) return q_hit->second.value.get();

    return nullptr;
  }

  void touch(const KeyT& key)
  {
    if (capacity_ == 1) return;

    auto s_hit = S_map_.find(key);

    if (s_hit != S_map_.end())
    {
      auto& entry = s_hit->second;
      if (entry.kind == Kind::LIR) 
      {
        //hit LIR
        S_.splice(S_.begin(), S_, entry.position);
        prune_stack();
        return;
      }

      if (entry.is_resident)
      {
        //hit ResHIR in S
        auto out_hit = Q_map_.find(key);
        promote_to_lir(entry, std::move(out_hit->second.value));
        hir_size_--;

        Q_erase(out_hit->second.position);
        back_lir_retirement();
        return;
      }
    }

    auto q_hit = Q_map_.find(key);

    if(q_hit != Q_map_.end()) 
    {
      //hit ResHir only in Q
      auto& entry = q_hit->second;
      Q_.splice(Q_.end(), Q_, entry.position);
      S_.push_front(key);
      S_map_.emplace(key, SEntry{nullptr, Kind::HIR, true, S_.begin()});
      return;
    }
  }

};

}  // namespace caches
module;

#include <algorithm>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <memory>
#include <utility>
#include <optional>

export module lirs_cache;
import abstract_cache;

export namespace caches {

template <typename Value, typename Key = int> 
class lirs_cache final : public abstract_cache<Value, Key>
{
  using EvictedEntry = typename abstract_cache<Value, Key>::EvictedEntry;
  using ListIt = typename std::list<Key>::iterator;

  const std::size_t capacity_;
  const std::size_t hir_capacity_;
  const std::size_t lir_capacity_;
  const std::size_t stack_limit_;
  
  std::size_t lir_size_ = 0;
  std::size_t hir_size_ = 0;

  enum class Kind { LIR, HIR };

  struct SEntry 
  {
    std::unique_ptr<Value> value;
    Kind kind;
    bool is_resident;
    ListIt position;
  };

  struct QEntry 
  {
    std::unique_ptr<Value> value;
    ListIt position;
  };

  std::list<Key> S_;  
  std::list<Key> Q_;

  std::unordered_map<Key, SEntry> S_map_; 
  std::unordered_map<Key, QEntry> Q_map_;

  std::optional<EvictedEntry> evict_if_full()
  {
    if (!full()) return std::nullopt;
    Key victim_key = Q_.front();
    EvictedEntry victim{victim_key, *Q_map_.at(victim_key).value};

    auto s_hit = S_map_.find(victim_key);
    if (s_hit != S_map_.end()) s_hit->second.is_resident = false;

    Q_map_.erase(victim_key);
    Q_.pop_front();

    hir_size_--;
    return victim;
  }

  void queue_push_back(const Key& key, std::unique_ptr<Value> value)
  {
    auto position = Q_.insert(Q_.end(), key);
    Q_map_.emplace(key, QEntry{std::move(value), position});
    ++hir_size_;
  }

  void promote_to_lir(SEntry& entry, std::unique_ptr<Value> value)
  {
    entry.value = std::move(value);
    entry.kind = Kind::LIR;
    entry.is_resident = true;
    ++lir_size_;
    S_.splice(S_.begin(), S_, entry.position);
  }

  std::optional<EvictedEntry> single_capacity_insert(const Key& key, const Value& value)
  {
    std::optional<EvictedEntry> victim = std::nullopt;
    if (hir_size_ == 1) 
    { 
      const Key& victim_key = Q_.front();
      victim.emplace(victim_key, *Q_map_.at(victim_key).value);
      Q_.pop_front(); 
      Q_map_.clear(); hir_size_--; 
    }
    
    queue_push_back(key, std::make_unique<Value>(value));
    return victim;
  }

  void prune_stack() 
  {
    while (!S_.empty() && S_map_.find(S_.back())->second.kind == Kind::HIR) 
    {
      S_map_.erase(S_.back());
      S_.pop_back();
    }
  }

  void trim_stack_to_limit()
  {
    auto it = S_.end();

    while (S_.size() > stack_limit_ && it != S_.begin())
    {
      --it;
      auto entry = S_map_.find(*it);

      if (entry->second.kind == Kind::HIR) {
        S_map_.erase(entry);
        it = S_.erase(it);
      }
    }
  }

  void demote_oldest_lir() 
  {
    Key victim_key = S_.back();
    auto& victim_entry = S_map_.find(victim_key)->second;

    queue_push_back(victim_key, std::move(victim_entry.value));
    S_map_.erase(victim_key);
    S_.pop_back();
    lir_size_--;

    prune_stack();
  }

  std::optional<EvictedEntry> insert_new_page (const Key& key, const Value& value)
  {
    auto victim = evict_if_full();

    if (lir_size_ == lir_capacity_) 
    {
      queue_push_back(key, std::make_unique<Value>(value));
      S_.push_front(key);
      S_map_.emplace(key, SEntry{nullptr, Kind::HIR, true, S_.begin()});
      trim_stack_to_limit();

      return victim;
    } 

    S_.push_front(key);
    S_map_.emplace(key, SEntry{std::make_unique<Value>(value), Kind::LIR, true, S_.begin()});
    trim_stack_to_limit();

    lir_size_++;
    return victim;
  }
  
  std::optional<EvictedEntry> insert(const Key& key, const Value& value) override
  {
    if (!capacity_) return std::nullopt;
    if (capacity_ == 1) return single_capacity_insert(key, value);

    auto s_hit = S_map_.find(key);
    if (s_hit != S_map_.end())
    {
      //hit NonResHir in S
      auto& entry = s_hit->second;
      auto victim = evict_if_full();
      promote_to_lir(entry, std::make_unique<Value>(value));
      if (lir_size_ > lir_capacity_) demote_oldest_lir();
      
      return victim;
    }

    //unknown hit
    return insert_new_page(key, value);
  }

  public:

  explicit lirs_cache(std::size_t capacity): 
    capacity_(capacity), 
    hir_capacity_(capacity == 0 ? 0 : std::max<std::size_t>(1, capacity / 100)),
    lir_capacity_(capacity_ - hir_capacity_),
    stack_limit_(capacity * 5) {}

  bool full() const override { return ((lir_size_ + hir_size_) == capacity_); }
  std::size_t capacity() const override { return capacity_; }

  const Value* peek(const Key& key) const override
  {
    auto s_hit = S_map_.find(key);
    if (s_hit != S_map_.end() && s_hit->second.kind == Kind::LIR) 
      return s_hit->second.value.get();

    auto q_hit = Q_map_.find(key);
    if (q_hit != Q_map_.end()) return q_hit->second.value.get();

    return nullptr;
  }

  void touch(const Key& key) override
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

        auto position = out_hit->second.position;
        Q_map_.erase(out_hit);
        Q_.erase(position);
        if (lir_size_ > lir_capacity_) demote_oldest_lir();
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
      prune_stack();
      trim_stack_to_limit();
      return;
    }
  }
  
  void invalidate(const Key& key) override
  {
    auto s_hit = S_map_.find(key);
    if (s_hit != S_map_.end())
    {
      const auto kind = s_hit->second.kind;
      auto& s_entry = s_hit->second;
      S_.erase(s_entry.position);
      S_map_.erase(s_hit);
      if (kind == Kind::LIR) { 
        prune_stack();
        --lir_size_; 
        return; 
      }
    }

    auto q_hit = Q_map_.find(key);
    if (q_hit != Q_map_.end())
    {
      auto& q_entry = q_hit->second;
      Q_.erase(q_entry.position);
      Q_map_.erase(key);
      --hir_size_;
    }
  }
};

}  // namespace caches

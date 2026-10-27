module;

#include <cstddef>
#include <list>
#include <unordered_map>
#include <optional>

export module two_queue_cache;
import abstract_cache;

export namespace caches {

template <typename Value, typename Key = int>
class two_queue_cache : public abstract_cache<Value, Key>
{
  using ListIt = typename std::list<Key>::iterator;

  enum class ResidentQueue {
    A1in,
    Am
  };

  struct Entry {
    Value value;
    ResidentQueue queue;
    ListIt position;
  };

  std::list<Key> a1in_;
  std::list<Key> a1out_;
  std::list<Key> am_;
  
  std::unordered_map<Key, Entry> entries_;
  std::unordered_map<Key, ListIt> history_;
  
  const std::size_t capacity_;
  const std::size_t capacity_Ain_ = capacity_ / 4;
  const std::size_t capacity_Aout_ = capacity_ / 2;

  const Key occupancy_solution() {
    if (a1in_.size() > capacity_Ain_) {
      Key victim_key = a1in_.back();
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

    Key victim_key = am_.back();
    entries_.erase(am_.back());
    am_.pop_back();
    return victim_key;
  }

  public:

  explicit two_queue_cache(std::size_t capacity): capacity_(capacity) {}
  bool full() const { return (entries_.size() == capacity_); }
  std::size_t capacity() const { return capacity_; }

  const Value* peek(const Key& key) const
  {
    auto hit = entries_.find(key);
    if (hit != entries_.end()) return &hit->second.value;
    return nullptr;
  }

  void touch(const Key& key)
  {
    auto hit = entries_.find(key);
    if (hit == entries_.end()) return;

    auto& entry = hit->second;
    if (entry.queue == ResidentQueue::Am) {
      am_.splice(am_.begin(), am_, entry.position);
    }
    //if hit in A1in we do nothing
  }

  void invalidate(const Key& key)
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

  private:

  std::optional<Key> insert(const Key& key, const Value& value) override
  {
    if (!capacity_) return std::nullopt;
    std::optional<Key> victim_key = std::nullopt;
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
};

} // namespace caches

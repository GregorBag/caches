module;

#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>

export module ideal_cache;

export namespace caches {

template <typename Key = int> class ideal_cache {
  const std::size_t capacity_;
  const std::size_t data_size_;
  const std::vector<Key> data_;

  using ListIt = typename std::list<Key>::iterator;

  std::list<Key> cache_;
  std::unordered_map<Key, ListIt> hash_;
  std::unordered_map<Key, std::list<std::size_t>> stats_;


  void cache_erase(ListIt it) 
  {
    hash_.erase(*it);
    cache_.erase(it);
  }

  std::size_t next_use(Key key)
  {
    if (stats_.at(key).empty()) return data_size_ + 1;
    return stats_.at(key).front();
  }

  ListIt cache_insert_key(Key key) 
  {
    ListIt currentIt = cache_.begin();
    std::size_t next = next_use(key);

    while (currentIt != cache_.end() && next >= next_use(*currentIt)) currentIt++;

    auto inserted = cache_.insert(currentIt, key);
    hash_.emplace(key, inserted);

    return inserted;
  }

  public:

  explicit ideal_cache(std::size_t capacity, std::size_t data_size, std::vector<Key> data): 
    capacity_(capacity), 
    data_size_(data_size), 
    data_(std::move(data)) 
    { 
      for (std::size_t i = 0; i < data_size_; i++) stats_[data_[i]].push_back(i); 
    }


  std::size_t ideal_hits_check() 
  {
    if (!capacity_) return 0;
    std::size_t hits = 0;

    for (std::size_t i = 0; i < data_size_; i++) 
    {
      Key key = data_[i];
      stats_.at(key).pop_front();
      
      auto hit = hash_.find(key);

      if (hit != hash_.end()) 
      {
        cache_erase(hit->second);
        cache_insert_key(key);

        hits++;
        continue;
      }

      if (cache_.size() >= capacity_) 
      {
        cache_erase(--cache_.end());
        cache_insert_key(key);
        continue;
      }
      cache_insert_key(key);
    }
    return hits;
  } 

}; 

} // namespace caches

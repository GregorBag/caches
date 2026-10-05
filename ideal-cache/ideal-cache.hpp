#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>

namespace caches {

template <typename KeyT = int> class ideal_cache_t {
  const std::size_t capacity_;
  const std::size_t data_size_;
  const std::vector<KeyT> data_;

  using ListIt = typename std::list<KeyT>::iterator;

  std::list<KeyT> cache_;
  std::unordered_map<KeyT, ListIt> hash_;
  std::unordered_map<KeyT, std::list<std::size_t>> stats_;


  void cache_delete_key(ListIt it) 
  {
    KeyT key = *it;
    cache_.erase(it);
    hash_.erase(key);
  }

  std::size_t next_use(KeyT key)
  {
    if (stats_.at(key).empty())
    {
      return data_size_ + 1;
    }
    return stats_.at(key).front();
  }

  ListIt cache_insert_key(KeyT key) 
  {
    ListIt currentIt = cache_.begin();
    std::size_t next = next_use(key);

    while (currentIt != cache_.end() && next >= next_use(*currentIt))
    {
      currentIt++;
    }

    auto inserted = cache_.insert(currentIt, key);
    hash_.emplace(key, inserted);

    return inserted;
  }


  public:

    explicit ideal_cache_t(std::size_t capacity, std::size_t data_size, std::vector<KeyT> data): 
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
      KeyT key = data_[i];
      stats_.at(key).pop_front();
      
      auto hit = hash_.find(key);

      if (hit != hash_.end()) 
      {
        cache_delete_key(hit->second);
        cache_insert_key(key);

        hits++;
        continue;
      }

      if (cache_.size() >= capacity_) 
      {
        cache_delete_key(--cache_.end());
        cache_insert_key(key);
        continue;
      }

      cache_insert_key(key);
    }

    return hits;
  } 

}; 

} // namespace caches
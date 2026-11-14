module;

#include <cstddef>
#include <iterator>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

export module ideal_cache;

export namespace caches {

template <typename Key> std::size_t ideal_hits_check(std::size_t capacity, std::vector<Key> data) 
{
  if (!capacity || data.empty()) return 0;

  std::vector<std::size_t> next(data.size());
  {
    std::unordered_map<Key, std::size_t> last;
    for (std::size_t i = data.size(); i-- > 0;) 
    {
      auto [it, inserted] = last.emplace(data[i], i);
      next[i] = inserted ? data.size() + i : it->second;
      it->second = i;
    }
  }

  std::set<std::size_t> cache;
  std::size_t hits = 0;

  for (std::size_t i = 0; i < data.size(); ++i) 
  {
    if (cache.erase(i))
      hits++;
    else if (cache.size() == capacity) 
      cache.erase(std::prev(cache.end()));

    cache.insert(next[i]);
  }
  return hits;
}

} // namespace caches

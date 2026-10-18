module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <optional>

export module abstract_cache;

export namespace caches {

template <typename Value, typename Key = int> class abstract_cache {
  public:

  template <typename F> bool lookup_update(Key key, F slow_get_page)
  {
    if (!capacity()) return false;
    if (peek(key))
    {
      touch(key);
      return true;
    }
    insert(key, slow_get_page(key));
    return false;
  }

  virtual const Value* peek(const Key& key) const = 0;
  virtual void touch(const Key& key) = 0;
  virtual std::optional<Key> insert(const Key& key, const Value& value) = 0;
  virtual void invalidate(const Key& key) = 0;
  virtual bool full() const = 0;
  virtual std::size_t capacity() const = 0;

  virtual ~abstract_cache() = default;
};

}
module;

#include <cstddef>
#include <optional>

export module abstract_cache;

export namespace caches {

template <typename Value, typename Key = int> class abstract_cache {
  virtual std::optional<Key> insert(const Key& key, const Value& value) = 0;

  protected:

  static std::optional<Key> insert_into(
    abstract_cache& cache, const Key& key, const Value& value)
  {
    return cache.insert(key, value);
  }

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
  virtual void invalidate(const Key& key) = 0;
  virtual bool full() const = 0;
  virtual std::size_t capacity() const = 0;
  
  abstract_cache() = default;

  abstract_cache(const abstract_cache&) = delete;
  abstract_cache& operator=(const abstract_cache&) = delete;

  abstract_cache(abstract_cache&&) = delete;
  abstract_cache& operator=(abstract_cache&&) = delete;

  virtual ~abstract_cache() = default;
};

}

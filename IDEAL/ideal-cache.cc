#include <cctype>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

#include "ideal-cache.hpp"

using PageId = long long;

bool read_integer(long long &value) {
  if (!(std::cin >> value))
    return false;
  // Reject tokens such as "12x"; reaching EOF after a number is valid.
  const auto next = std::cin.peek();
  return !std::cin.bad() &&
         (next == std::char_traits<char>::eof() ||
          std::isspace(static_cast<unsigned char>(next)));
}

int main() {
  long long m, n;
  if (!read_integer(m) || !read_integer(n) || m < 0 || n < 0 ||
      static_cast<unsigned long long>(m) > std::numeric_limits<std::size_t>::max() ||
      static_cast<unsigned long long>(n) > std::numeric_limits<std::size_t>::max()) {
    std::cerr << "Expected nonnegative cache size and request count\n";
    return 1;
  }

  std::vector<PageId> data;
  for (long long i = 0; i < n; ++i) {
    PageId key;
    if (!read_integer(key)) {
      std::cerr << "Expected a page key\n";
      return 1;
    }
    data.push_back(key);
  }

  caches::ideal_cache_t<PageId> cache{static_cast<std::size_t>(m),
                                    static_cast<std::size_t>(n), std::move(data)};
  const auto hits = cache.ideal_hits_check();

  std::cout << hits << std::endl;
  return std::cout ? 0 : 1;
}

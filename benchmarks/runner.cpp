#include <CLI/CLI.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

import multi_level_cache;
import ideal_cache;

using Cache = caches::multi_level_cache<int>;
using Type = caches::CacheTypes;
using Level = std::pair<Type, std::size_t>;

std::unique_ptr<Cache> make_cache(const std::vector<std::string>& names,
                                 const std::vector<int>& sizes) {
  const std::map<std::string, Type> types{
      {"lru", Type::LRU}, {"lfu", Type::LFU}, {"arc", Type::ARC},
      {"lirs", Type::LIRS}, {"2q", Type::TWO_QUEUE}};
  std::vector<Level> levels;
  for (std::size_t i = 0; i < names.size(); ++i)
    levels.emplace_back(types.at(names[i]), sizes[i]);
  switch (levels.size()) {
    case 1: return std::make_unique<Cache>(std::initializer_list<Level>{levels[0]});
    case 2: return std::make_unique<Cache>(std::initializer_list<Level>{levels[0], levels[1]});
    case 3: return std::make_unique<Cache>(std::initializer_list<Level>{levels[0], levels[1], levels[2]});
    default: throw std::invalid_argument("Expected 1 to 3 levels");
  }
}

int main(int argc, char** argv) {
  CLI::App app{"Run one cache configuration; print hits and request count"};
  std::string path;
  std::vector<std::string> names{"lru"};
  std::vector<int> sizes;
  bool ideal = false;
  app.add_option("--trace", path)->required()->check(CLI::ExistingFile);
  app.add_option("--algorithms", names)->expected(1, 3)
      ->check(CLI::IsMember({"lru", "lfu", "arc", "lirs", "2q"}));
  app.add_option("--capacities", sizes)->required()->expected(1, 3)->check(CLI::PositiveNumber);
  app.add_flag("--ideal", ideal, "Run IDEAL with one capacity instead");
  CLI11_PARSE(app, argc, argv);
  try {
    if ((ideal && sizes.size() != 1) || (!ideal && names.size() != sizes.size()))
      throw std::invalid_argument("Provide one capacity per level (one for IDEAL)");
    if (!std::is_sorted(sizes.begin(), sizes.end()))
      throw std::invalid_argument("Capacities must be nondecreasing");
    std::ifstream input(path);
    int count, key;
    if (!(input >> count) || count <= 0)
      throw std::invalid_argument("Trace must begin with a positive request count");
    std::vector<int> keys;
    for (int i = 0; i < count; ++i) {
      if (!(input >> key)) throw std::invalid_argument("Missing or invalid trace key");
      keys.push_back(key);
    }
    input >> std::ws;
    if (input.peek() != std::char_traits<char>::eof())
      throw std::invalid_argument("Extra data after declared requests");
    std::size_t hits = 0;
    if (ideal) {
      hits = caches::ideal_hits_check(sizes[0], keys);
    } else {
      auto cache = make_cache(names, sizes);
      for (int value : keys)
        hits += cache->lookup_update(value, [](int k) { return k; });
    }
    std::cout << hits << ' ' << keys.size() << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

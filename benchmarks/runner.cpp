#include <CLI/CLI.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

import multi_level_cache;
import ideal_cache;

namespace {

using Cache = caches::multi_level_cache<int>;
using Type = caches::CacheTypes;
using Level = std::pair<Type, std::size_t>;

constexpr std::pair<const char*, Type> algorithms[]{
    {"lru", Type::LRU}, {"lfu", Type::LFU}, {"arc", Type::ARC},
    {"lirs", Type::LIRS}, {"2q", Type::TWO_QUEUE}};

struct Options {
  std::string path;
  std::vector<int> sizes;
  bool ideal = false;
};

void parse_arguments(CLI::App& app, Options& options, int argc, char** argv) {
  app.add_option("--trace", options.path)->required()->check(CLI::ExistingFile);
  app.add_option("--capacities", options.sizes)->required()->expected(1, 3)->check(CLI::PositiveNumber);
  app.add_flag("--ideal", options.ideal, "Run only IDEAL with one capacity");
  app.parse(argc, argv);
  if (options.ideal && options.sizes.size() != 1)
    throw std::invalid_argument("Provide one capacity for IDEAL");
  if (!std::is_sorted(options.sizes.begin(), options.sizes.end()))
    throw std::invalid_argument("Capacities must be nondecreasing");
}

std::vector<int> read_trace(const std::string& path) {
  std::ifstream input(path);
  int universe_size, count;
  if (!(input >> universe_size >> count) || universe_size <= 0 || count <= 0)
    throw std::invalid_argument("Trace must begin with positive universe size and request count");
  std::vector<int> keys;
  for (int i = 0, key; i < count; ++i) {
    if (!(input >> key)) throw std::invalid_argument("Missing or invalid trace key");
    keys.push_back(key);
  }
  input >> std::ws;
  if (input.peek() != std::char_traits<char>::eof())
    throw std::invalid_argument("Extra data after declared requests");
  return keys;
}

Cache make_cache(const std::vector<Level>& levels) {
  switch (levels.size()) {
    case 1: return Cache{levels[0]};
    case 2: return Cache{levels[0], levels[1]};
    case 3: return Cache{levels[0], levels[1], levels[2]};
    default: throw std::invalid_argument("Expected 1 to 3 levels");
  }
}

std::size_t count_hits(Cache& cache, const std::vector<int>& keys) {
  std::size_t hits = 0;
  for (int key : keys)
    hits += cache.lookup_update(key, [](int k) { return k; });
  return hits;
}

void print_result(const std::string& name, std::size_t hits, std::size_t count) {
  std::cout << name << ' ' << hits << ' ' << count << '\n';
}

void run_all(const std::vector<int>& keys, const std::vector<int>& sizes,
             std::vector<Level> levels = {}, const std::string& name = "") {
  if (levels.size() == sizes.size()) {
    auto cache = make_cache(levels);
    print_result(name, count_hits(cache, keys), keys.size());
    return;
  }
  for (const auto& [algorithm, type] : algorithms) {
    levels.emplace_back(type, sizes[levels.size()]);
    run_all(keys, sizes, levels, name.empty() ? algorithm : name + ">" + algorithm);
    levels.pop_back();
  }
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  CLI::App app{"Run all algorithm combinations; print strategy, hits and request count"};
  try {
    parse_arguments(app, options, argc, argv);
    const auto keys = read_trace(options.path);
    if (options.ideal)
      print_result("IDEAL", caches::ideal_hits_check(options.sizes[0], keys), keys.size());
    else
      run_all(keys, options.sizes);
  } catch (const CLI::ParseError& error) {
    return app.exit(error);
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

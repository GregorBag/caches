#include <CLI/CLI.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

import multi_level_inclusive_cache;
import multi_level_exclusive_cache;
import ideal_cache;

namespace {

using Type = caches::CacheTypes;
using Level = std::pair<Type, std::size_t>;

constexpr std::pair<const char*, Type> algorithms[]{
    {"lru", Type::LRU}, {"lfu", Type::LFU}, {"arc", Type::ARC},
    {"lirs", Type::LIRS}, {"2q", Type::TWO_QUEUE}};

struct Options {
  std::string path;
  std::vector<int> sizes;
  bool ideal = false;
  bool inclusive = false;
};

void parse_arguments(CLI::App& app, Options& options, int argc, char** argv) {
  app.add_option("--trace", options.path, "Trace file, or - to read stdin")->required();
  app.add_option("--capacities", options.sizes)->required()->expected(1, 3)->check(CLI::PositiveNumber);
  app.add_flag("--ideal", options.ideal, "Run only IDEAL with one capacity");
  app.add_flag("--inclusive", options.inclusive, "Use an inclusive hierarchy instead of exclusive")
      ->excludes("--ideal");
  app.add_flag("--exclusive", "Use an exclusive hierarchy (default)")
      ->excludes("--ideal")->excludes("--inclusive");
  app.parse(argc, argv);
  if (options.ideal && options.sizes.size() != 1)
    throw std::invalid_argument("Provide one capacity for IDEAL");
  if (!std::is_sorted(options.sizes.begin(), options.sizes.end()))
    throw std::invalid_argument("Capacities must be nondecreasing");
}

std::vector<int> read_trace(std::istream& input) {
  int universe_size, count;
  if (!(input >> universe_size >> count) || universe_size <= 0 || count <= 0)
    throw std::invalid_argument("Trace must begin with positive universe size and request count");
  std::vector<int> keys;
  keys.reserve(count);
  for (int i = 0, key; i < count; ++i) {
    if (!(input >> key)) throw std::invalid_argument("Missing or invalid trace key");
    if (key < 1 || key > universe_size)
      throw std::invalid_argument("Trace key outside declared universe");
    keys.push_back(key);
  }
  input >> std::ws;
  if (input.peek() != std::char_traits<char>::eof())
    throw std::invalid_argument("Extra data after declared requests");
  return keys;
}

template <typename Cache>
Cache make_cache(const std::vector<Level>& levels) {
  switch (levels.size()) {
    case 1: return Cache{levels[0]};
    case 2: return Cache{levels[0], levels[1]};
    case 3: return Cache{levels[0], levels[1], levels[2]};
    default: throw std::invalid_argument("Expected 1 to 3 levels");
  }
}

template <typename Cache>
std::size_t count_hits(Cache& cache, const std::vector<int>& keys) {
  std::size_t hits = 0;
  for (int key : keys)
    hits += cache.lookup_update(key, [](int k) { return k; });
  return hits;
}

void print_result(const std::string& name, std::size_t hits, std::size_t count) {
  std::cout << name << ' ' << hits << ' ' << count << '\n';
}

template <typename Cache>
void run_all(const std::vector<int>& keys, const std::vector<int>& sizes,
             std::vector<Level> levels = {}, const std::string& name = "") {
  if (levels.size() == sizes.size()) {
    auto cache = make_cache<Cache>(levels);
    print_result(name, count_hits(cache, keys), keys.size());
    return;
  }
  for (const auto& [algorithm, type] : algorithms) {
    levels.emplace_back(type, sizes[levels.size()]);
    run_all<Cache>(keys, sizes, levels, name.empty() ? algorithm : name + ">" + algorithm);
    levels.pop_back();
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
  Options options;
  CLI::App app{"Run all algorithm combinations; print strategy, hits and request count"};
  try {
    parse_arguments(app, options, argc, argv);
    std::ifstream file;
    if (options.path != "-") {
      file.open(options.path);
      if (!file) throw std::invalid_argument("Cannot open trace: " + options.path);
    }
    const auto keys = read_trace(options.path == "-" ? std::cin : file);
    if (options.ideal)
      print_result("IDEAL", caches::ideal_hits_check(options.sizes[0], keys), keys.size());
    else if (options.inclusive)
      run_all<caches::multi_level_inclusive_cache<int>>(keys, options.sizes);
    else
      run_all<caches::multi_level_exclusive_cache<int>>(keys, options.sizes);
  } catch (const CLI::ParseError& error) {
    return app.exit(error);
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

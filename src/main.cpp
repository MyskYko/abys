#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "abys/frontend.h"
#include "abys/version.h"

namespace {

bool is_version_flag(const std::string &arg) {
  return arg == "-v" || arg == "--version";
}

bool is_help_flag(const std::string &arg) {
  return arg == "-h" || arg == "--help";
}

void print_help() {
  std::cout << "abys: logic synthesis toolchain (scaffold)\n";
  std::cout << "Usage:\n";
  std::cout << "  abys --version\n";
  std::cout << "  abys parse <files...> [--top <module>]\n";
}

} // namespace

int main(int argc, char **argv) {
  if (argc <= 1) {
    print_help();
    return 0;
  }

  for (int i = 1; i < argc; ++i) {
    if (is_version_flag(argv[i])) {
      std::cout << "abys " << abys::version() << '\n';
      return 0;
    }
    if (is_help_flag(argv[i])) {
      print_help();
      return 0;
    }
  }

  std::string command = argv[1];
  if (command == "parse") {
    std::vector<std::string> files;
    std::optional<std::string> top;

    for (int i = 2; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--top" && i + 1 < argc) {
        top = argv[++i];
        continue;
      }
      files.push_back(arg);
    }

    auto result = abys::parse_systemverilog(files, top);
    if (!result.ok) {
      std::cerr << "parse failed: " << result.message << '\n';
      return 2;
    }

    std::cout << "parse ok\n";
    return 0;
  }

  print_help();
  return 1;
}

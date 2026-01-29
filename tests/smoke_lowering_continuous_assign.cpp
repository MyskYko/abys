#include <optional>
#include <string>
#include <vector>

#include "abys/frontend.h"

int main() {
  std::vector<std::string> files = {"../tests/fixtures/and_gate.sv"};
  std::optional<std::string> top = "and_gate";
  auto result = abys::build_tig_from_systemverilog(files, top);
  return result.ok ? 0 : 1;
}

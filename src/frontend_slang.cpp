#include "abys/frontend.h"
#include "abys/ir/lowering_slang.h"
#include "abys/ir/tig.h"
#include "abys/ir/tig_builder.h"

#include "slang/driver/Driver.h"

namespace abys {

ParseResult parse_systemverilog(const std::vector<std::string> &files,
                                const std::optional<std::string> &top) {
  if (files.empty()) {
    return {false, "no input files provided"};
  }

  slang::driver::Driver driver;

  for (const auto &file : files) {
    driver.sourceLoader.addFiles(file);
  }

  if (top && !top->empty()) {
    driver.options.topModules.push_back(*top);
  }

  if (!driver.processOptions()) {
    return {false, "failed to process slang options"};
  }

  if (!driver.parseAllSources()) {
    return {false, "failed to parse SystemVerilog sources"};
  }

  auto compilation = driver.createCompilation();
  if (!compilation) {
    return {false, "failed to create slang compilation"};
  }

  driver.reportCompilation(*compilation, true);
  if (driver.diagEngine.getNumErrors() > 0) {
    return {false, "slang reported compilation errors"};
  }

  return {true, "ok"};
}

ir::TigBuildResult build_tig_from_systemverilog(const std::vector<std::string> &files,
                                                const std::optional<std::string> &top) {
  ir::Tig design;

  if (files.empty()) {
    return {false, "no input files provided", {}};
  }

  slang::driver::Driver driver;

  for (const auto &file : files) {
    driver.sourceLoader.addFiles(file);
  }

  if (top && !top->empty()) {
    driver.options.topModules.push_back(*top);
  }

  if (!driver.processOptions()) {
    return {false, "failed to process slang options", {}};
  }

  if (!driver.parseAllSources()) {
    return {false, "failed to parse SystemVerilog sources", {}};
  }

  auto compilation = driver.createCompilation();
  if (!compilation) {
    return {false, "failed to create slang compilation", {}};
  }

  driver.reportCompilation(*compilation, true);
  if (driver.diagEngine.getNumErrors() > 0) {
    return {false, "slang reported compilation errors", {}};
  }

  ir::TigBuilder builder(design);
  ir::lower_slang_ast_to_ir(compilation->getRoot(), builder);

  return {true, "ok", std::move(design)};
}

} // namespace abys

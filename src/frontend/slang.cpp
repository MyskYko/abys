#include "abys/frontend/api.h"
#include "abys/frontend/slang_lowering.h"
#include "abys/ir/tig.h"
#include "abys/ir/tig_builder.h"

#include "slang/driver/Driver.h"

namespace abys {

namespace {

void add_default_translate_off_formats(slang::driver::Driver &driver) {
  driver.options.translateOffOptions.push_back("synopsys,translate_off,translate_on");
  driver.options.translateOffOptions.push_back("synthesis,translate_off,translate_on");
  driver.options.translateOffOptions.push_back("pragma,translate_off,translate_on");
}

} // namespace

ParseResult parse_systemverilog(const std::vector<std::string> &files, const std::optional<std::string> &top) {
  if (files.empty()) {
    return {false, "no input files provided"};
  }

  slang::driver::Driver driver;
  driver.addStandardArgs();
  add_default_translate_off_formats(driver);

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

ir::TigBuildResult build_tig_from_systemverilog(const std::vector<std::string> &files, const std::optional<std::string> &top) {
  ir::Tig design;

  if (files.empty()) {
    return {false, "no input files provided", {}};
  }

  slang::driver::Driver driver;
  driver.addStandardArgs();
  add_default_translate_off_formats(driver);

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
  if (top && !top->empty()) {
    builder.set_top_module(*top);
  }
  frontend::lower_slang_ast_to_ir(compilation->getRoot(), builder);

  return {true, "ok", std::move(design)};
}

} // namespace abys

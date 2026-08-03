#include "abys/infra/diagnostics.h"

#include <ostream>
#include <stdexcept>
#include <utility>

namespace abys {

std::string_view diagnostic_message(DiagnosticId id) {
  switch (id) {
  case DiagnosticId::kLoweringUnresolvedSignalInput:
    return "leaving unresolved signal input unconnected";
  case DiagnosticId::kLoweringLevelSensitiveEventIgnored:
    return "ignoring level-sensitive event in edge-sensitive procedural block";
  case DiagnosticId::kLoweringMixedAssignmentTreatedAsNonblocking:
    return "treating mixed blocking/nonblocking assignments as nonblocking";
  case DiagnosticId::kLoweringSystemFunctionReplacedWithZero:
    return "replacing non-synthesizable system function with zero";
  case DiagnosticId::kLoweringSystemCallIgnored:
    return "ignoring non-synthesizable system call";
  case DiagnosticId::kLoweringFullCaseWithDefaultIgnored:
    return "ignoring full_case on case statement with default";
  case DiagnosticId::kLoweringUnconnectedInputPort:
    return "leaving unconnected input port";
  case DiagnosticId::kLoweringProceduralBlockIgnored:
    return "ignoring procedural block";
  case DiagnosticId::kLoweringTaskIgnored:
    return "ignoring task in synthesis lowering";
  case DiagnosticId::kFrontendUnhandledSynthesisAttribute:
    return "unhandled synthesis attribute";
  case DiagnosticId::kFrontendUnhandledSynthesisComment:
    return "unhandled synthesis comment";
  }
  throw std::logic_error("Unknown diagnostic ID");
}

std::string Diagnostics::format(const Entry &entry) {
  std::string result(diagnostic_message(entry.id));
  if (!entry.detail.empty()) {
    result += ": " + entry.detail;
  }
  return result;
}

Diagnostics::Diagnostics(std::ostream &output) : output_(&output) {}

void Diagnostics::warning(DiagnosticId id, std::string detail) {
  entries_.push_back({DiagnosticLevel::kWarning, id, std::move(detail)});
  if (output_) {
    *output_ << "warning: " << format(entries_.back()) << '\n';
  }
}

} // namespace abys

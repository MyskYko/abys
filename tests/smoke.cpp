#include "abys/version.h"

int main() {
  return abys::version().empty() ? 1 : 0;
}

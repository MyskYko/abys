#include "catch_amalgamated.hpp"

#include "frontend/lower_sv.h"

TEST_CASE("lower a continuous assignment", "[frontend]") {
  const auto output = abys::test::lower_and_dump(R"(
module top(input logic [7:0] a, b, output logic [7:0] y);
  assign y = a + b;
endmodule
)");

  CHECK(output == R"(module top (
  input [7:0] a,
  input [7:0] b,
  output  logic [7:0] y);



  always @(*)   begin
    logic [7:0] abys_dumper_tmp4;
    abys_dumper_tmp4 = (a + b);
    y = abys_dumper_tmp4;
  end
endmodule
)");
}

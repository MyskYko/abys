#include "catch_amalgamated.hpp"

#include "frontend/lower_sv.h"

TEST_CASE("lower a flip-flop with asynchronous reset", "[frontend]") {
  const auto output = abys::test::lower_and_dump(R"(
module top(
  input logic clk,
  input logic rst_n,
  input logic [7:0] d,
  output logic [7:0] q
);
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)
      q <= '0;
    else
      q <= d;
  end
endmodule
)");

  CHECK(output == R"(module top (
  input clk,
  input rst_n,
  input [7:0] d,
  output  logic [7:0] q);



  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      begin
        q <= 8'b0;
      end
    end else begin
      begin
        q <= d;
      end
    end
  end
endmodule
)");
}

module top (
  input [7:0] a,
  input [7:0] b,
  output  logic [15:0] y);



  always @(*)   begin
    logic [15:0] abys_dumper_tmp3;
    logic [15:0] abys_dumper_tmp5;
    logic [15:0] abys_dumper_tmp6;
    abys_dumper_tmp3 = $unsigned(a);
    abys_dumper_tmp5 = $unsigned(b);
    abys_dumper_tmp6 = (abys_dumper_tmp3 * abys_dumper_tmp5);
    y = abys_dumper_tmp6;
  end
endmodule

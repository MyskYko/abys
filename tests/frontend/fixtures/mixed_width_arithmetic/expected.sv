module top (
  input signed [7:0] a,
  input [3:0] b,
  output  logic signed [8:0] y);



  always @(*)   begin
    logic [8:0] abys_dumper_tmp3;
    logic [8:0] abys_dumper_tmp5;
    logic [8:0] abys_dumper_tmp6;
    logic signed [8:0] abys_dumper_tmp7;
    abys_dumper_tmp3 = $unsigned(a);
    abys_dumper_tmp5 = $unsigned(b);
    abys_dumper_tmp6 = (abys_dumper_tmp3 + abys_dumper_tmp5);
    abys_dumper_tmp7 = $signed(abys_dumper_tmp6);
    y = abys_dumper_tmp7;
  end
endmodule

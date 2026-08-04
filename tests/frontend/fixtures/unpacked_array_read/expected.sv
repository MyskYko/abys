module top (
  input [7:0] a [3:0],
  input [1:0] index,
  output  logic [7:0] y);



  always @(*)   begin
    logic [1:0] abys_dumper_tmp5;
    logic [7:0] abys_dumper_tmp6;
    abys_dumper_tmp5 = (2'b11 - index);
    abys_dumper_tmp6 = a[abys_dumper_tmp5];
    y = abys_dumper_tmp6;
  end
endmodule

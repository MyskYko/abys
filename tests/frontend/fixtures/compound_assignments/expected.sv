module top (
  input [7:0] a,
  input [2:0] shift,
  output  logic [7:0] y);



  always @(*)   begin
    logic [7:0] abys_dumper_tmp4;
    logic [7:0] abys_dumper_tmp5;
    logic [7:0] abys_dumper_tmp7;
    logic [7:0] abys_dumper_tmp8;
    logic [7:0] abys_dumper_tmp10;
    logic [7:0] abys_dumper_tmp11;
    logic [7:0] abys_dumper_tmp13;
    logic [7:0] abys_dumper_tmp14;
    logic [7:0] abys_dumper_tmp16;
    logic [7:0] abys_dumper_tmp17;
    logic [7:0] abys_dumper_tmp19;
    logic [7:0] abys_dumper_tmp21;
    abys_dumper_tmp4 = $unsigned(8'b11);
    abys_dumper_tmp5 = (a + abys_dumper_tmp4);
    abys_dumper_tmp7 = $unsigned(8'b1);
    abys_dumper_tmp8 = (abys_dumper_tmp5 - abys_dumper_tmp7);
    abys_dumper_tmp10 = $unsigned(8'b11111110);
    abys_dumper_tmp11 = (abys_dumper_tmp8 & abys_dumper_tmp10);
    abys_dumper_tmp13 = $unsigned(8'b10000);
    abys_dumper_tmp14 = (abys_dumper_tmp11 | abys_dumper_tmp13);
    abys_dumper_tmp16 = $unsigned(8'b1010101);
    abys_dumper_tmp17 = (abys_dumper_tmp14 ^ abys_dumper_tmp16);
    abys_dumper_tmp19 = (abys_dumper_tmp17 << shift);
    abys_dumper_tmp21 = (abys_dumper_tmp19 >> 32'sb1);
    y = abys_dumper_tmp21;
  end
endmodule

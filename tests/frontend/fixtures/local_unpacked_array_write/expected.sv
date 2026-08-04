module top (
  input [1:0] index,
  input [7:0] value,
  output  logic [7:0] y);

  logic [7:0] memory_abys_block0 [3:0];


  always @(*)   begin
    logic [7:0] abys_dumper_tmp36 [3:0];
    logic [31:0] abys_dumper_tmp6;
    logic [7:0] abys_dumper_tmp3;
    logic [31:0] abys_dumper_tmp14;
    logic [7:0] abys_dumper_tmp11;
    logic [31:0] abys_dumper_tmp21;
    logic [7:0] abys_dumper_tmp18;
    logic [31:0] abys_dumper_tmp28;
    logic [7:0] abys_dumper_tmp25;
    logic [1:0] abys_dumper_tmp34;
    logic [31:0] abys_dumper_tmp39;
    logic [7:0] abys_dumper_tmp40;
    abys_dumper_tmp6 = (2'b11 - 32'sb0);
    abys_dumper_tmp3 = $unsigned(8'b10001);
    abys_dumper_tmp36[abys_dumper_tmp6] = abys_dumper_tmp3;
    abys_dumper_tmp14 = (2'b11 - 32'sb1);
    abys_dumper_tmp11 = $unsigned(8'b100010);
    abys_dumper_tmp36[abys_dumper_tmp14] = abys_dumper_tmp11;
    abys_dumper_tmp21 = (2'b11 - 32'sb10);
    abys_dumper_tmp18 = $unsigned(8'b110011);
    abys_dumper_tmp36[abys_dumper_tmp21] = abys_dumper_tmp18;
    abys_dumper_tmp28 = (2'b11 - 32'sb11);
    abys_dumper_tmp25 = $unsigned(8'b1000100);
    abys_dumper_tmp36[abys_dumper_tmp28] = abys_dumper_tmp25;
    abys_dumper_tmp34 = (2'b11 - index);
    abys_dumper_tmp36[abys_dumper_tmp34] = value;
    abys_dumper_tmp39 = (2'b11 - 32'sb10);
    abys_dumper_tmp40 = abys_dumper_tmp36[abys_dumper_tmp39];
    y = abys_dumper_tmp40;
  end
endmodule

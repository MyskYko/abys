module top (
  input row,
  input column,
  input [7:0] value,
  output  logic [7:0] y);

  logic [7:0] memory [1:0] [1:0];


  always @(*)   begin
    logic [7:0] abys_dumper_tmp46 [1:0] [1:0];
    logic [31:0] abys_dumper_tmp8;
    logic [31:0] abys_dumper_tmp5;
    logic [7:0] abys_dumper_tmp3;
    logic [31:0] abys_dumper_tmp18;
    logic [31:0] abys_dumper_tmp15;
    logic [7:0] abys_dumper_tmp13;
    logic [31:0] abys_dumper_tmp27;
    logic [31:0] abys_dumper_tmp24;
    logic [7:0] abys_dumper_tmp22;
    logic [31:0] abys_dumper_tmp36;
    logic [31:0] abys_dumper_tmp33;
    logic [7:0] abys_dumper_tmp31;
    logic abys_dumper_tmp44;
    logic abys_dumper_tmp41;
    logic [31:0] abys_dumper_tmp48;
    logic [7:0] abys_dumper_tmp49 [1:0];
    logic [31:0] abys_dumper_tmp51;
    logic [7:0] abys_dumper_tmp52;
    abys_dumper_tmp46 = memory;
    abys_dumper_tmp8 = (1'b1 - 32'sb0);
    abys_dumper_tmp5 = (1'b1 - 32'sb0);
    abys_dumper_tmp3 = $unsigned(8'b10001);
    abys_dumper_tmp46[abys_dumper_tmp8][abys_dumper_tmp5] = abys_dumper_tmp3;
    abys_dumper_tmp18 = (1'b1 - 32'sb0);
    abys_dumper_tmp15 = (1'b1 - 32'sb1);
    abys_dumper_tmp13 = $unsigned(8'b100010);
    abys_dumper_tmp46[abys_dumper_tmp18][abys_dumper_tmp15] = abys_dumper_tmp13;
    abys_dumper_tmp27 = (1'b1 - 32'sb1);
    abys_dumper_tmp24 = (1'b1 - 32'sb0);
    abys_dumper_tmp22 = $unsigned(8'b110011);
    abys_dumper_tmp46[abys_dumper_tmp27][abys_dumper_tmp24] = abys_dumper_tmp22;
    abys_dumper_tmp36 = (1'b1 - 32'sb1);
    abys_dumper_tmp33 = (1'b1 - 32'sb1);
    abys_dumper_tmp31 = $unsigned(8'b1000100);
    abys_dumper_tmp46[abys_dumper_tmp36][abys_dumper_tmp33] = abys_dumper_tmp31;
    abys_dumper_tmp44 = (1'b1 - row);
    abys_dumper_tmp41 = (1'b1 - column);
    abys_dumper_tmp46[abys_dumper_tmp44][abys_dumper_tmp41] = value;
    abys_dumper_tmp48 = (1'b1 - 32'sb1);
    abys_dumper_tmp49 = abys_dumper_tmp46[abys_dumper_tmp48];
    abys_dumper_tmp51 = (1'b1 - 32'sb0);
    abys_dumper_tmp52 = abys_dumper_tmp49[abys_dumper_tmp51];
    y = abys_dumper_tmp52;
    memory[abys_dumper_tmp8][abys_dumper_tmp5] = abys_dumper_tmp3;
    memory[abys_dumper_tmp18][abys_dumper_tmp15] = abys_dumper_tmp13;
    memory[abys_dumper_tmp27][abys_dumper_tmp24] = abys_dumper_tmp22;
    memory[abys_dumper_tmp36][abys_dumper_tmp33] = abys_dumper_tmp31;
    memory[abys_dumper_tmp44][abys_dumper_tmp41] = value;
  end
endmodule

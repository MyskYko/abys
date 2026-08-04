module top (
  input select,
  input [7:0] a,
  input [7:0] b,
  output  logic [7:0] y);

  logic [7:0] memory [1:0];


  always @(*)   begin
    logic [7:0] abys_dumper_tmp23 [1:0];
    logic [31:0] abys_dumper_tmp4;
    logic [31:0] abys_dumper_tmp10;
      logic [31:0] abys_dumper_tmp15;
      logic [31:0] abys_dumper_tmp19;
    logic [31:0] abys_dumper_tmp25;
    logic [7:0] abys_dumper_tmp26;
    logic [31:0] abys_dumper_tmp28;
    logic [7:0] abys_dumper_tmp29;
    logic [7:0] abys_dumper_tmp30;
    abys_dumper_tmp23 = memory;
    abys_dumper_tmp4 = (1'b1 - 32'sb0);
    abys_dumper_tmp23[abys_dumper_tmp4] = a;
    abys_dumper_tmp10 = (1'b1 - 32'sb1);
    abys_dumper_tmp23[abys_dumper_tmp10] = b;
      abys_dumper_tmp15 = (1'b1 - 32'sb0);
      abys_dumper_tmp19 = (1'b1 - 32'sb1);
    if (select) begin
      abys_dumper_tmp23[abys_dumper_tmp4] = a;
      abys_dumper_tmp23[abys_dumper_tmp10] = b;
      abys_dumper_tmp23[abys_dumper_tmp15] = b;
    end else begin
      abys_dumper_tmp23[abys_dumper_tmp4] = a;
      abys_dumper_tmp23[abys_dumper_tmp10] = b;
      abys_dumper_tmp23[abys_dumper_tmp19] = a;
    end
    abys_dumper_tmp25 = (1'b1 - 32'sb0);
    abys_dumper_tmp26 = abys_dumper_tmp23[abys_dumper_tmp25];
    abys_dumper_tmp28 = (1'b1 - 32'sb1);
    abys_dumper_tmp29 = abys_dumper_tmp23[abys_dumper_tmp28];
    abys_dumper_tmp30 = (abys_dumper_tmp26 ^ abys_dumper_tmp29);
    y = abys_dumper_tmp30;
    memory[abys_dumper_tmp4] = a;
    memory[abys_dumper_tmp10] = b;
    if (select) begin
      memory[abys_dumper_tmp4] = a;
      memory[abys_dumper_tmp10] = b;
      memory[abys_dumper_tmp15] = b;
    end else begin
      memory[abys_dumper_tmp4] = a;
      memory[abys_dumper_tmp10] = b;
      memory[abys_dumper_tmp19] = a;
    end
  end
endmodule

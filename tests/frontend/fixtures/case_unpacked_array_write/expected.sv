module top (
  input [1:0] select,
  input [7:0] a,
  output  logic [7:0] y);

  logic [7:0] memory [2:0];


  always @(*)   begin
    logic [7:0] abys_dumper_tmp46 [2:0];
    logic [31:0] abys_dumper_tmp6;
    logic [7:0] abys_dumper_tmp3;
    logic [31:0] abys_dumper_tmp14;
    logic [7:0] abys_dumper_tmp11;
    logic [31:0] abys_dumper_tmp21;
    logic [7:0] abys_dumper_tmp18;
    logic [1:0] abys_dumper_tmp26;
      logic [31:0] abys_dumper_tmp30;
    logic [1:0] abys_dumper_tmp34;
      logic [31:0] abys_dumper_tmp37;
      logic [31:0] abys_dumper_tmp42;
    logic [31:0] abys_dumper_tmp49;
    logic [7:0] abys_dumper_tmp50;
    abys_dumper_tmp46 = memory;
    abys_dumper_tmp6 = (2'b10 - 32'sb0);
    abys_dumper_tmp3 = $unsigned(8'b10001);
    abys_dumper_tmp46[abys_dumper_tmp6] = abys_dumper_tmp3;
    abys_dumper_tmp14 = (2'b10 - 32'sb1);
    abys_dumper_tmp11 = $unsigned(8'b100010);
    abys_dumper_tmp46[abys_dumper_tmp14] = abys_dumper_tmp11;
    abys_dumper_tmp21 = (2'b10 - 32'sb10);
    abys_dumper_tmp18 = $unsigned(8'b110011);
    abys_dumper_tmp46[abys_dumper_tmp21] = abys_dumper_tmp18;
    abys_dumper_tmp26 = $unsigned(2'b0);
      abys_dumper_tmp30 = (2'b10 - 32'sb0);
    abys_dumper_tmp34 = $unsigned(2'b1);
      abys_dumper_tmp37 = (2'b10 - 32'sb1);
      abys_dumper_tmp42 = (2'b10 - 32'sb10);
    case (select)
    abys_dumper_tmp26: begin
      abys_dumper_tmp46[abys_dumper_tmp6] = abys_dumper_tmp3;
      abys_dumper_tmp46[abys_dumper_tmp14] = abys_dumper_tmp11;
      abys_dumper_tmp46[abys_dumper_tmp21] = abys_dumper_tmp18;
      abys_dumper_tmp46[abys_dumper_tmp30] = a;
    end
    abys_dumper_tmp34: begin
      abys_dumper_tmp46[abys_dumper_tmp6] = abys_dumper_tmp3;
      abys_dumper_tmp46[abys_dumper_tmp14] = abys_dumper_tmp11;
      abys_dumper_tmp46[abys_dumper_tmp21] = abys_dumper_tmp18;
      abys_dumper_tmp46[abys_dumper_tmp37] = a;
    end
    default: begin
      abys_dumper_tmp46[abys_dumper_tmp6] = abys_dumper_tmp3;
      abys_dumper_tmp46[abys_dumper_tmp14] = abys_dumper_tmp11;
      abys_dumper_tmp46[abys_dumper_tmp21] = abys_dumper_tmp18;
      abys_dumper_tmp46[abys_dumper_tmp42] = a;
    end
    endcase
    abys_dumper_tmp49 = (2'b10 - 32'sb1);
    abys_dumper_tmp50 = abys_dumper_tmp46[abys_dumper_tmp49];
    y = abys_dumper_tmp50;
    memory[abys_dumper_tmp6] = abys_dumper_tmp3;
    memory[abys_dumper_tmp14] = abys_dumper_tmp11;
    memory[abys_dumper_tmp21] = abys_dumper_tmp18;
    case (select)
    abys_dumper_tmp26: begin
      memory[abys_dumper_tmp6] = abys_dumper_tmp3;
      memory[abys_dumper_tmp14] = abys_dumper_tmp11;
      memory[abys_dumper_tmp21] = abys_dumper_tmp18;
      memory[abys_dumper_tmp30] = a;
    end
    abys_dumper_tmp34: begin
      memory[abys_dumper_tmp6] = abys_dumper_tmp3;
      memory[abys_dumper_tmp14] = abys_dumper_tmp11;
      memory[abys_dumper_tmp21] = abys_dumper_tmp18;
      memory[abys_dumper_tmp37] = a;
    end
    default: begin
      memory[abys_dumper_tmp6] = abys_dumper_tmp3;
      memory[abys_dumper_tmp14] = abys_dumper_tmp11;
      memory[abys_dumper_tmp21] = abys_dumper_tmp18;
      memory[abys_dumper_tmp42] = a;
    end
    endcase
  end
endmodule

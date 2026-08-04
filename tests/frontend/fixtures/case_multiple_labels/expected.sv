module top (
  input [2:0] selector,
  input [7:0] a,
  input [7:0] b,
  output  logic [7:0] y);



  always @(*)   begin
    logic [2:0] abys_dumper_tmp4;
    logic [2:0] abys_dumper_tmp6;
    logic [2:0] abys_dumper_tmp10;
    logic [2:0] abys_dumper_tmp12;
      logic [7:0] abys_dumper_tmp15;
    abys_dumper_tmp4 = $unsigned(3'b0);
    abys_dumper_tmp6 = $unsigned(3'b10);
    abys_dumper_tmp10 = $unsigned(3'b1);
    abys_dumper_tmp12 = $unsigned(3'b11);
      abys_dumper_tmp15 = (a ^ b);
    case (selector)
    abys_dumper_tmp4, abys_dumper_tmp6: begin
      y = a;
    end
    abys_dumper_tmp10, abys_dumper_tmp12: begin
      y = b;
    end
    default: begin
      y = abys_dumper_tmp15;
    end
    endcase
  end
endmodule

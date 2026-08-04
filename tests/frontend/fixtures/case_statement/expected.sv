module top (
  input [1:0] select,
  input [7:0] a,
  input [7:0] b,
  output  logic [7:0] y);



  always @(*)   begin
    logic [1:0] abys_dumper_tmp4;
    logic [1:0] abys_dumper_tmp7;
    abys_dumper_tmp4 = $unsigned(2'b0);
    abys_dumper_tmp7 = $unsigned(2'b1);
    case (select)
    abys_dumper_tmp4: begin
      y = a;
    end
    abys_dumper_tmp7: begin
      y = b;
    end
    default: begin
      y = 8'b0;
    end
    endcase
  end
endmodule

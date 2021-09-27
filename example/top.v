module top
  (input wire 	     i_clk,
   input wire [15:0] i_sw,
   output reg [15:0] o_led);

   integer 	     idx;

   reg [3:0] 	     ones;

   always @(i_sw) begin
      ones = 4'd0;
      for (idx = 0 ; idx < 16 ; idx = idx + 1)
	ones = ones + {3'd0, i_sw[idx]};
   end

   always @(posedge i_clk)
     o_led <= (1<<ones)-1;

endmodule 
   
     

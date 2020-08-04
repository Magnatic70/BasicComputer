module ram_dual(
		output reg [7:0] characterOut,
		output reg [7:0] colorOut,
		input [7:0] characterIn,
		input [7:0]	colorIn,
		input [12:0] addr_in,
		input [12:0] addr_out,
		input we,
		input clk1,
		input clk2
	);
 
   reg [7:0] q;
   reg [7:0] characters [8191:0];
	reg [7:0] colorInfo [8191:0];
	initial $readmemh("bootscreen.mem",characters);
	initial $readmemh("bootcolor.mem",colorInfo);
	
   always @(posedge clk1)
      if (we) begin
         characters[addr_in] <= characterIn;
			colorInfo[addr_in] <= colorIn;
		end
			
   always @(negedge clk2) begin
      characterOut <= characters[addr_out];
		colorOut <= colorInfo[addr_out];
	end
        
endmodule

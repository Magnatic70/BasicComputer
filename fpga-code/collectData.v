module collectData(
		input clock,
		input [7:0]received,
		input ready,
		input computerRunning,
		output reg [12:0] pos,
		output reg writeEnable,
		output reg [7:0] character,
		output reg [7:0] color
	);
	reg [1:0]state;

	// Handle incoming serial data
	always @(posedge ready) begin
		case (received)
			8'b11111111: 	begin
									state<=2'b00;
									writeEnable<=1'b0;
								end
			default: case (state)
							2'b00: 	begin
											pos[12:6]<=received[6:0];
											state=2'b01;
											writeEnable<=1'b0;
										end
							2'b01: 	begin
											pos[5:0]<=received[5:0];
											state=2'b10;
											writeEnable<=1'b0;
										end
							2'b10: 	begin
											state<=2'b11;
											character<=received;
											writeEnable<=1'b0;
										end
							2'b11:	begin
											color<=received;
											writeEnable<=1'b1;
										end
						endcase
		endcase
		if(!computerRunning)
			state<=2'b00;
	end
endmodule

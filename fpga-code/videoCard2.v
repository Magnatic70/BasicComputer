module vgaOutputNew(
		input clk,						// 23.75 Mhz for VGA 640x480
		input [7:0] character,
		input [2:0] fgColor,
		input [2:0] bgColor,
		output [2:0] pixel,
		output hsyncOut,
		output vsyncOut,
		output [12:0] characterPos
	);
	
	reg [9:0] counterX; // x of pixel
	reg [8:0] counterY; // y of pixel
	initial counterX=0;
	initial counterY=0;
	
	reg [7:0] fontRom [1023:0];
	initial $readmemh("8x8-font-hex-2.mem",fontRom);
	
	wire inDisplayArea;
	wire [2:0] fontPixel;
	wire [7:0] fontRow;
	wire [10:0] fontAddress;
	
	assign characterPos[12:6]=counterX[9:3];
	assign characterPos[5:0]=counterY[8:3];
	assign inDisplayArea=(counterX<640 && counterY<480);
	assign fontPixel=counterX[2:0];
	assign fontAddress[10:3]=(character>=32 && character<=127) ? character : 32;
	assign fontAddress[2:0]=counterY[2:0];
	assign fontRow=fontRom[fontAddress];
	assign pixel=inDisplayArea ? (fontRow[fontPixel] ? fgColor : bgColor) : 3'b000;

	assign hsyncOut=(counterX >= (640 + 16) && (counterX <= (640 + 16 + 96)));
	assign vsyncOut=(counterY >= (480 + 3) && (counterY <= (480 + 3 + 4)));
	
	always @(posedge clk) begin
		if(counterX==799) begin
			counterX<=0;
			counterY=counterY+1;
		end
		else
			counterX<=counterX+1;
		if(counterY>499)
			counterY<=0;
	end			
endmodule

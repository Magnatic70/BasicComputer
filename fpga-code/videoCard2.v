module hvsync_generator(
    input clk,
    output vga_h_sync,
    output vga_v_sync,
    output reg inDisplayArea,
    output reg [9:0] CounterX,
    output reg [9:0] CounterY
  );
    reg vga_HS, vga_VS;

    wire CounterXmaxed = (CounterX == 840);
    wire CounterYmaxed = (CounterY == 500);

    always @(posedge clk)
    if (CounterXmaxed)
      CounterX <= 0;
    else
      CounterX <= CounterX + 1;

    always @(posedge clk)
    begin
      if (CounterXmaxed)
      begin
        if(CounterYmaxed)
          CounterY <= 0;
        else
          CounterY <= CounterY + 1;
      end
    end

    always @(posedge clk)
    begin
      vga_HS <= (CounterX > (640 + 16) && (CounterX < (640 + 16 + 64)));   // active for 64 clocks
      vga_VS <= (CounterY > (480 + 1) && (CounterY < (480 + 1 + 3)));   // active for 1 clocks
    end

    always @(posedge clk)
    begin
        inDisplayArea <= (CounterX < 640) && (CounterY < 480);
    end

    assign vga_h_sync = ~vga_HS;
    assign vga_v_sync = ~vga_VS;

endmodule

module vgaOutputNew(
		input clk,						// 31.5 Mhz for VGA 640x480 at 75 Hz
		input [7:0] character,
		input [2:0] fgColor,
		input [2:0] bgColor,
		output reg [2:0] pixel,
		output hsyncOut,
		output vsyncOut,
		output [12:0] characterPos
	);

    wire inDisplayArea;
    wire [9:0] counterX;
    wire [9:0] counterY;
	 reg [7:0] fontRom [1023:0];
	 initial $readmemh("8x8-font-hex-2.mem",fontRom);
	
	 reg [2:0] fontPixel;
	 reg [7:0] fontRow;
	 reg [10:0] fontAddress;

    hvsync_generator hvsync(
      .clk(clk),
      .vga_h_sync(hsyncOut),
      .vga_v_sync(vsyncOut),
      .CounterX(counterX),
      .CounterY(counterY),
      .inDisplayArea(inDisplayArea)
    );
	 
	 assign characterPos[12:6]=counterX[9:3];
	 assign characterPos[5:0]=counterY[8:3];

	always @(posedge clk)
    begin
      if (inDisplayArea) begin
		  fontPixel=counterX[2:0]-1;
		  fontAddress[10:3]=(character>=32 && character<=127) ? character : 32;
		  fontAddress[2:0]=counterY[2:0];
		  fontRow=fontRom[fontAddress];
        pixel <= fontRow[fontPixel] ? fgColor : bgColor;
		end
      else // if it's not to display, go dark
        pixel <= 3'b000;
    end
endmodule

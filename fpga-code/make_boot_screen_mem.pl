#!/usr/bin/perl
$mx=128;
$my=64;
$welcome="Magnatic Basic Computer (C)2020. Please wait while booting...";
open(FILE,'>bootscreen.mem');
for($x=0;$x<$mx;$x++){
	for($y=0;$y<$my;$y++){
		if($y==0 && substr($welcome,$x,1)){
			print FILE unpack "H*",(substr($welcome,$x,1));
		}
		else{
			print FILE '00';
		}
		print FILE " ";
	}
	print FILE "\n";
}

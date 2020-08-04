#!/usr/bin/perl
$mx=128;
$my=64;
$welcome="Magnatic Basic Computer (C)2020. Please wait while booting...";
open(FILE,'>bootcolor.mem');
for($x=0;$x<$mx;$x++){
	for($y=0;$y<$my;$y++){
		print FILE '0F ';
	}
	print FILE "\n";
}

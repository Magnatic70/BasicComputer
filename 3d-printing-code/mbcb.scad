include <../magnatic-library.scad>

$fn=128;

bigChamfer=7;
width=130;
height=43.5;
radiusOverWidth=9;
radius=width/2+radiusOverWidth;
boardXOffset=0.5;
boardYOffset=13.5;

module case(){
  difference(){
    union(){
      chamferCube(width,width,height,bigChamfer);
      translate([width/2,width/2,0]){
        myCylinder(radius,height,bigChamfer);
      }
    }
    translate([width/2,width/2,height-2]){
      hollowCylinder(2.1,width/2,width/2-2,0);
    }
    translate([width/2,width/2,height-1]){
      myText("MBC",36,1.1,128);
    }
    translate([bigChamfer,bigChamfer,bigChamfer]){
      cube([113,102+16,33]); // Chamber for board
    }
    translate([bigChamfer+boardXOffset+2,bigChamfer+boardYOffset+101,bigChamfer+3.5]){
      cube([64,20,17.5]); // Hole for connectors
    }
    translate([bigChamfer+38,0,bigChamfer]){
      cube([42,8,33]); // Chamber for ESP
    }
    
    // Holes for screws
    translate([bigChamfer/4,width/2,-0.1]){
      cylinder(r=4.8/2,h=bigChamfer-2);
    }
    translate([width-bigChamfer/4,width/2,-0.1]){
      cylinder(r=4.8/2,h=bigChamfer-2);
    }
    translate([bigChamfer/4,width/2,-0.1]){
      cylinder(r=1.8/2,h=bigChamfer+19);
    }
    translate([width-bigChamfer/4,width/2,-0.1]){
      cylinder(r=1.8/2,h=bigChamfer+19);
    }
  }
}

module ac4e6e10(){
  color("Green"){
    cube([112,101,20]);
  }
  translate([32,101,5]){
    color("Blue"){
      cube([31,7,14]);
    }
  }
  translate([80,101,5]){
    color("Black"){
      cube([31,7,14]);
    }
  }
}
  
module caseUnderside(){
  difference(){
    case();
    translate([-0.1-radiusOverWidth,-0.1-radiusOverWidth,bigChamfer]){
      cube([width+0.2+2*radiusOverWidth,width+0.2+2*radiusOverWidth,height-bigChamfer+0.1]);
    }
    // Screw holes for board placement
    translate([bigChamfer+boardXOffset,bigChamfer+boardYOffset,3.1]){
      translate([5,9,0]){
        cylinder(r=0.7,h=5);
      }
      translate([5,80,0]){
        cylinder(r=0.7,h=5);
      }
      translate([112-7,80,0]){
        cylinder(r=0.7,h=5);
      }
      translate([112-7,9,0]){
        cylinder(r=0.7,h=5);
      }
    }
    translate([width/2,101,bigChamfer-0.5]){
      myText("Connectors",8,0.6,16);
    }
  }
}

module caseTop(){
  difference(){
    case();
    translate([-0.1-radiusOverWidth,-0.1-radiusOverWidth,-0.1]){
      cube([width+0.2+2*radiusOverWidth,width+0.2+2*radiusOverWidth,bigChamfer+0.11]);
    }
  }
}

caseUnderside();

//caseTop();

translate([bigChamfer+boardXOffset,bigChamfer+boardYOffset,bigChamfer]){
  //ac4e6e10();
}

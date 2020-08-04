module myCylinder(radius,height,chamfer){
  union(){
    cylinder(h=chamfer,r1=radius-chamfer,r2=radius);
    translate([0,0,chamfer]){
      cylinder(h=height-2*chamfer,r=radius);
    }
    translate([0,0,height-chamfer]){
      cylinder(h=chamfer,r1=radius,r2=radius-chamfer);
    }
  }
}

module myCylinderCTop(radius,height,chamfer){
  union(){
    cylinder(h=height-chamfer,r=radius);
    translate([0,0,height-chamfer]){
      cylinder(h=chamfer,r1=radius,r2=radius-chamfer);
    }
  }
}

module myCylinderCBottom(radius,height,chamfer){
  union(){
    cylinder(h=chamfer,r1=radius-chamfer,r2=radius);
    translate([0,0,chamfer]){
      cylinder(h=height-chamfer,r=radius);
    }
  }
}

module hollowCylinder(height,outerRadius,innerRadius,chamfer){
  difference(){
    myCylinder(outerRadius,height,chamfer);
    union(){
      translate([0,0,-0.001]){
        cylinder(h=chamfer+0.002,r1=innerRadius+chamfer,r2=innerRadius);
      }
      translate([0,0,chamfer-0.001]){
        cylinder(h=height-2*chamfer+0.002,r=innerRadius);
      }
      translate([0,0,height-chamfer-0.001]){
        cylinder(h=chamfer+0.002,r1=innerRadius,r2=innerRadius+chamfer);
      }
    }
  }
}

module chamferCube(sizeX, sizeY, sizeZ, chamferHeight, center, chamferX, chamferY, chamferZ) {
  chamferX = (chamferX == undef) ? [1, 1, 1, 1] : chamferX;
  chamferY = (chamferY == undef) ? [1, 1, 1, 1] : chamferY;
  chamferZ = (chamferZ == undef) ? [1, 1, 1, 1] : chamferZ;
  chamferCLength = sqrt(chamferHeight * chamferHeight * 2);
  tx=center?-(sizeX/2):0;
  ty=center?-(sizeY/2):0;
  tz=center?-(sizeZ/2):0;
  translate([tx,ty,tz]){
    difference() {
      cube([sizeX, sizeY, sizeZ]);
      for(x = [0 : 3]) {
        chamferSide1 = min(x, 1) - floor(x / 3); // 0 1 1 0
        chamferSide2 = floor(x / 2); // 0 0 1 1
        if(chamferX[x]) {
          translate([-0.1, chamferSide1 * sizeY, -chamferHeight + chamferSide2 * sizeZ]){
            rotate([45, 0, 0]){
              cube([sizeX + 0.2, chamferCLength, chamferCLength]);
            }
          }
        }
        if(chamferY[x]) {
          translate([-chamferHeight + chamferSide2 * sizeX, -0.1, chamferSide1 * sizeZ]){
            rotate([0, 45, 0]){
              cube([chamferCLength, sizeY + 0.2, chamferCLength]);
            }
          }
        }
        if(chamferZ[x]) {
          translate([chamferSide1 * sizeX, -chamferHeight + chamferSide2 * sizeY, -0.1]){
            rotate([0, 0, 45]){
              cube([chamferCLength, chamferCLength, sizeZ + 0.2]);
            }
          }
        }
      }
    }
  }
}

module wedge(width1,width2,depth,height){
  linear_extrude(height = height) {
    polygon(points=[[0,0],[width2,0],[width1,depth]]);
  }
}

module torus(r1, r2){
  rotate_extrude(){
    translate([r1,0,0]){
      circle(r2);
    }
  }
}

module myText(line,size,depth,detail){
  linear_extrude(height = depth) {
    text(line, size = size, font = "Liberation Sans", halign = "center", valign = "center", $fn = detail);
  }
}

module myPolyhedron(points){ // Simplified version of polyhedron, suitable for 6-sided polyhedrons
  // Define points as follows: Begin at 0,0 than counter clockwise all points for the underside than the upside
  faces=[
    [0,1,2,3],  // bottom
    [4,5,1,0],  // front
    [7,6,5,4],  // top
    [5,6,2,1],  // right
    [6,7,3,2],  // back
    [7,4,0,3]   // left
  ];
  polyhedron(points=points,faces=faces);
}

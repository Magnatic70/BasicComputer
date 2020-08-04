10 OPEN ("readwrite")
20 PRINT read$(1);
30 IF EOF=0 THEN GOTO 20
40 CLOSE

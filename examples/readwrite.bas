5 ERASE "readwrite"
10 OPEN ("readwrite")
20 WRITE "test"
30 RSEEK (0)
40 PRINT READLINE$
50 WSEEK (1)
60 WRITE "a";
70 RSEEK (0)
80 PRINT READLINE$
90 CLOSE

10 REM string functions
20 A$ = "Hello, " + "World"
30 PRINT LEFT$(A$, 5); "|"; RIGHT$(A$, 5); "|"; MID$(A$, 3, 4)
40 PRINT LEN(A$), INSTR(A$, "World"), UCASE$(A$)
50 B$ = STRING$(3, "*") + CHR$(65) + STR$(ASC("B"))
60 PRINT B$; VAL("12.5") * 2
70 PRINT USING "###.##"; 3.14159

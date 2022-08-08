# Assembling your MIPS
1. Compile the cpp.
```
g++ mips2hex.cpp
```
2. Write MIPS on a text file, and pass it to the executable you compiled.
```
./a.out mips.txt
```
    
It will generate `hex.txt`, which will contain 4 digit hexadecimals for your mips code. 
For debugging it will also generate `hex_debug.txt`, where instruction to instruction mapping is shown.
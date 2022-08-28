# Assembling your MIPS

## Linux
1. Put your mips code in `mips_input.txt`. 
2. Run `buildnrun.sh`

## Windows
1. Compile `mips2hex.cpp`. 
```
g++ -std=c++20 mips2hex.cpp
```

2. Write your mips code in a text file, say `mips_input.txt`. 

3. Execute the compiled file, passing in your mips text file. 
```
./a.exe mips_input.txt
```

# Output
Executing the assembler will generate `hex.txt`, which will contain 4 digit hexadecimals for your mips code. 
For debugging it will also generate `hex_debug.txt`, where mips to hex mapping is shown line by line. 
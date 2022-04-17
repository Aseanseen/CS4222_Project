# How to run
1. Clone the project into the contiki folder. This is important as it will affect the `Makefile` and `cooja_project.csc`.
2. In `cooja_project.csc`, for lines with `EXPORT`, ensure that the directory chosen is correct.

## Option: Running on Cooja
3. Run `./compile_cooja`
4. Locate the `cooja_project.csc` file and open the simulation in cooja.
5. Right click on the mote to find the option to move the mote during testing.
6. Filter the word "DETECT" or "ABSENT" to see overview.

## Option: Running on SensorTag
3. Run `./compile_sensortag`.
4. Use Uniflash to flash the tokens with `token.bin`.
5. Run `./run.sh` to observe the output.

# How the code works
- There is a hash table that stores `TokenData`. The key is the node id.
- Every new token that has never been detected by the token before will be stored in the hash table. This is up to a maximum number of tokens, which is defined in `def_and_types.h`
- There are 2 modes, absent and detect. The token is initially absent.
- A cycle refers to one complete round of the send and sleep slots. At the start of every new cycle, process the information of the last cycle. This means counting if there are consecutive cycles.
- Upon reaching the consecutive number of cycles, change state. Absent: if there are 15 seconds worth of cycles where the other token has been detected, change to detect mode. Detect: if there are 30 seconds worth of cycles where the other token is absent, change to absent mode.
- Upon changing mode, prints the **Timestamp (in seconds) DETECT/ABSENT nodeID** of the first occurence of the consecutives.


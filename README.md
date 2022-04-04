# How to run
1. Clone the project into the contiki folder. This is important as it will affect the `Makefile` and `cooja_project.csc`. 
2. Find the node ID of each token, this will be reflected when reading the output from the token. Refer to assignment-1 if unsure. Token refers to the SensorTag.
3. In `constants.h`, change the value of `TOKEN_1_ADDR`, `TOKEN_2_ADDR` to their respective values.
4. For token 1, switch the order of the first 2 hexadecimals with the last 2 hexadecimals and compute the value. e.g. If `TOKEN_1_ADDR = 0x5403`, compute the decimal value of `0x0354`.
5. In `cooja_project.csc`, find the mote with `motetype_identifier` of `sky1` and change the field of id to the decimal value computed in step 2.
6. Repeat steps 2-3 for token 2, `motetype_identifier` of `sky2`.
7. In `cooja_project.csc`, for lines with `EXPORT`, ensure that the directory chosen is correct.

## Option: Running on Cooja
8. Run `./compile_cooja`
9. Locate the `cooja_project.csc` file and open the simulation in cooja.
10. Right click on the mote to find the option to move the mote during testing.

## Option: Running on SensorTag
8. Run `./compile_sensortag`.
9. Use Uniflash to flash the tokens with their respective binary files.
10. Run `./run.sh` to observe the output.

Note: For step 3-4, Cooja appears to compute the address differently. Hence, these steps will help to set the right address for the mote.

# How the code works
- There are 2 modes, absent and detect. The node is initially absent.
- A cycle refers to one complete round of the send and sleep slots. At the start of every new cycle, process the information of the last cycle. This means counting if there are consecutive cycles.
- Upon reaching the consecutive number of cycles, change state. Absent: if there are 15 seconds worth of cycles where the other token has been detected, change to detect mode. Detect: if there are 30 seconds worth of cycles where the other token is absent, change to absent mode.
- Upon changing mode, prints the **Timestamp (in seconds) DETECT/ABSENT nodeID** of the first occurence of the consecutives.

# To do
- Code a bash script to edit `cooja_project.csc` and `constants.h` given 2 node ids as inputs
- Change the value of RSSI_THRESHOLD_3M in constants.h to the appropriate value
- Combine the code to determine proximity based on RSSI
- Find the best setting for power saving
- Measure energy consumption using Cooja
- Maybe change the `DETECT_TO_ABSENT`, `ABSENT_TO_DETECT` to change based on the actual time of 30s and 15s, rather than the current implementation which assumes each cycle lasts for 1s.

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
- There are 2 modes, absent and detect. The token is initially absent.
- A cycle refers to one complete round of the send and sleep slots. At the start of every new cycle, process the information of the last cycle. This means counting if there are consecutive cycles.
- Upon reaching the consecutive number of cycles, change state. Absent: if there are 15 seconds worth of cycles where the other token has been detected, change to detect mode. Detect: if there are 30 seconds worth of cycles where the other token is absent, change to absent mode.
- Upon changing mode, prints the **Timestamp (in seconds) DETECT/ABSENT nodeID** of the first occurence of the consecutives.

# To do
- Change the value of RSSI_THRESHOLD_3M in constants.h to the appropriate value
- Combine the code to determine proximity based on RSSI
- Find the best setting for power saving
- Measure energy consumption using Cooja
- Maybe change the `DETECT_TO_ABSENT`, `ABSENT_TO_DETECT` to change based on the actual time of 30s and 15s, rather than the current implementation which assumes each cycle lasts for 1s.

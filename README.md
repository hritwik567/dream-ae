# DREAM Artifact

## Steps to compile and run
- Clone this repository

- Compile DRAMSim3
```
cd DRAMsim3
mkdir build
cd build
cmake ..
make -j
```

- Compile memsim
```
cd memsim
make -j
```

- Download traces from Google Drive, [link](https://drive.google.com/file/d/1QP80z2VjxC0ph26wYVko_q9REGv2Znql/view?usp=sharing). You can also use the `gdown` utility to directly download it from the terminal.
```
pip3 install gdown

``` 


- After downloading traces, the directory structure should look like this:
```
.
├── DRAMsim3/
├── memsim/
├── README.md
├── runall.sh
├── scripts/
└── traces/
```

- From the base directory, run all the configurations using
```
./runall.sh
# You can update the MAX_JOBS variable to update the maximum number of parallel jobs you want to spawn
# You can also use the parallel utility to spread the jobs across muiltiple nodes, see the commented code
```

## Steps to generate plots after all the configs have finished running

- Collect the stats from all the generated results
```
./genstats.sh # this should generate a stats folder
```

- Plot the figures using the following commands
```
python3 scripts/gen_fig3.py # Output: fig3.pdf
python3 scripts/gen_fig6.py # Output: fig6.pdf
python3 scripts/gen_fig8.py # Output: fig8.pdf
python3 scripts/gen_fig15.py # Output: fig15.pdf
python3 scripts/gen_fig17.py # Output: fig17.pdf
```
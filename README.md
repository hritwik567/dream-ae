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

- Download traces in the trace folder

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
python3 scripts/gen_fig3.py
python3 scripts/gen_fig6.py
python3 scripts/gen_fig8.py
python3 scripts/gen_fig15.py
python3 scripts/gen_fig17.py
```
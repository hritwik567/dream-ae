#! /bin/bash
BASE_DIR=RESULTS/
MAX_JOBS=150
# ./scripts/run.sh -cfg DRAMsim3/configs/fig3/ -out $BASE_DIR/RESULT_FIG3 -lim $MAX_JOBS
# ./scripts/run.sh -cfg DRAMsim3/configs/fig8/ -out $BASE_DIR/RESULT_FIG8 -lim $MAX_JOBS
# ./scripts/run.sh -cfg DRAMsim3/configs/fig15/ -out $BASE_DIR/RESULT_FIG15 -lim $MAX_JOBS
# ./scripts/run.sh -cfg DRAMsim3/configs/fig17/ -out $BASE_DIR/RESULT_FIG17 -lim $MAX_JOBS

# wait


# Use the following code to run the jobs using GNU Parallel
./scripts/run.sh -cfg DRAMsim3/configs/fig3/ -out $BASE_DIR/RESULT_FIG3 -par
./scripts/run.sh -cfg DRAMsim3/configs/fig8/ -out $BASE_DIR/RESULT_FIG8 -par
./scripts/run.sh -cfg DRAMsim3/configs/fig15/ -out $BASE_DIR/RESULT_FIG15 -par
./scripts/run.sh -cfg DRAMsim3/configs/fig17/ -out $BASE_DIR/RESULT_FIG17 -par

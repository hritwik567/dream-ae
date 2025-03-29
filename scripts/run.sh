#!/bin/bash
# set -x

# Maximum number of simultaneous jobs
MAX_JOBS=160

# Function to wait for jobs to finish when limit is reached
function wait_for_jobs {
    while [ $(jobs -r | wc -l) -ge $MAX_JOBS ]; do
        sleep 1
    done
}

# Base options
_1b="1000000000"
_100m="100000000"
_20m="20000000"
_250m="250000000"
base_opt="-inst_limit ${_250m}  -ratemode 8 -dramsim3cfg"

# allow option -local to run locally
local=1
parallel=0
bin_name="sim_dramsim3"
while [[ "$#" -gt 0 ]]; do
  case $1 in
    -cfg|--config_folder)
      config_folder="$2"
      echo "Config Folder: $config_folder"
      shift 2
      ;;
    -out|--output_folder)
      output_folder="$2"
      echo "Output Folder: $output_folder"
      shift 2
      ;;  
    -par|--parallel)
      echo "Running using Parallel"
      parallel=1
      local=0
      shift
      ;;
    -lim|--limit)
      MAX_JOBS=$2
      echo "Max Jobs: $MAX_JOBS"
      shift 2
      ;;
    -m|--mem)
      mem=$2
      base_opt="-memsize $mem $base_opt"
      echo "Memory: $mem"
      shift 2
      ;;
    -p|--prefix)
      global_prefix=$2
      echo "Prefix: $global_prefix"
      shift 2
      ;;
    *)
      echo "Unknown option: $1"
      shift
      ;;
  esac
done

if [ -z "$config_folder" ]; then
    echo "Please provide a config folder"
    exit
fi

if [ -z "$output_folder" ]; then
    echo "Please provide an output folder"
    exit
fi

mkdir -p $output_folder

opts=()
out_files=()
# if config folder is a file, then use that file
if [ -f $config_folder ]; then
    echo "Config: $config_folder"
    rel_path=$(realpath --relative-to=$output_folder $config_folder)
    echo "Relative Path: $rel_path"
    opts+=("$base_opt $rel_path")
    out_files+=($(basename $config_folder | sed 's/DDR5_32Gb_//g' | sed 's/.ini//g')) # remove DDR5_32Gb_x4_4800_ 
else
  for cfg in $(ls $config_folder/*.ini)
  do
      echo "Config: $cfg"
      rel_path=$(realpath --relative-to=$output_folder $cfg)
      echo "Relative Path: $rel_path"
      opts+=("$base_opt $rel_path")
      out_files+=($(basename $cfg | sed 's/DDR5_32Gb_//g' | sed 's/.ini//g')) # remove DDR5_32Gb_x4_4800_ 
  done
fi
# Current directory
BASE=$(pwd)
TRACES_DIR="${BASE}/traces/"

# Prioritize traces with "_17" in the name
priority_traces=($(ls $TRACES_DIR/*_17.mtf.gz 2>/dev/null))
other_traces=($(ls $TRACES_DIR/*.mtf.gz | grep -v "_17" 2>/dev/null))

# Combine priority and other traces
all_traces=("${priority_traces[@]}" "${other_traces[@]}")

mkdir -p $output_folder
cd $output_folder

cmd_tmpfile=$(mktemp)

for trace in "${all_traces[@]}"
do
    full_trace=$(realpath $trace)
    name=$(basename $trace)
    echo "Trace: $name"
    for i in $(seq 0 $((${#opts[@]} - 1)))
    do
        prefix="${out_files[$i]}_$name"
        if [ ! -z "$global_prefix" ]; then
            prefix="${global_prefix}_${prefix}"
        fi

        if [ $local -eq 1 ]; then
            nohup $BASE/memsim/$bin_name ${opts[$i]} $full_trace > $prefix.out 2>&1 &
            wait_for_jobs
        elif [ $parallel -eq 1 ]; then
            echo "$BASE/memsim/$bin_name ${opts[$i]} $full_trace > $prefix.out 2>&1" >> $cmd_tmpfile
        fi
    done
done

cat $cmd_tmpfile 

set -x
if [ $parallel -eq 1 ]; then
    nohup parallel --sshloginfile .. --sshdelay 0.1 --workdir $PWD -a $cmd_tmpfile bash -c &
fi

cd -

#! /bin/bash

mkdir -p stats

python3 scripts/stats.py RESULTS/RESULT_FIG3 -baseline mop4_sb -gmean -dropna -out stats/fig3.csv
python3 scripts/stats.py RESULTS/RESULT_FIG8 RESULTS/RESULT_FIG3 -noslowdown -pivot MITIG_UTIL -mean -dropna -out stats/fig6.csv
python3 scripts/stats.py RESULTS/RESULT_FIG8 RESULTS/RESULT_FIG3 -baseline mop4_sb -gmean -dropna -out stats/fig8.csv
python3 scripts/stats.py RESULTS/RESULT_FIG15 -baseline mop4_sb -gmean -dropna -out stats/fig15.csv
python3 scripts/stats.py RESULTS/RESULT_FIG15 RESULTS/RESULT_FIG17 -baseline mop4_sb -gmean -dropna -out stats/fig17.csv

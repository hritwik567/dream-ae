import pandas as pd
import matplotlib.pyplot as plt
import matplotlib
from matplotlib import gridspec
matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42
plt.rcParams['font.family'] = 'serif'
plt.rcParams.update({'font.size': 10})

from common import get_key, insert_empty_rows, slowdown, sort_based_on_suite

# read mulitiple csv files
data = pd.read_csv('stats/fig15.csv')
data['FILE'] = data['FILE'].apply(get_key)
data = data.replace('GEOMEAN', 'Gmean')

data = sort_based_on_suite(data, ['spec2k17', 'stream4', 'gap6'])

data = insert_empty_rows(data, [11, 15, 21])

fig = plt.figure(figsize=(12, 4))
gs = gridspec.GridSpec(2, 1, height_ratios=[2, 2])  # Adjust height_ratios as needed
axs = [None, None, None]
axs[0] = fig.add_subplot(gs[0])  # First subplot (smaller)
axs[1] = fig.add_subplot(gs[1])  # Second subplot (medium)

ymax = 100

# Plot bar graph
bar_width = 0.3
index = range(len(data))
colors = ['#5C4033', '#FBC740', '#2CA02C', '#FF7F0E', '#0E86D4', '#9467bd']

axs[0].bar([i + 0*bar_width for i in index], slowdown(data['mop4_dream_setassoc4_trhd500']), bar_width, label='Set-Assoc-Grouping (TRH=500)', edgecolor='black', color=colors[0], zorder=2)
axs[0].bar([i + 1*bar_width for i in index], slowdown(data['mop4_dream_rand4_trhd500']), bar_width, label='Random-Grouping   (TRH=500)', edgecolor='black', color=colors[3], zorder=2)

clip_indices = [5, 8]  # Assuming lbm and parest are at index 2 and 3
for i in clip_indices:
    axs[0].text(i, 50 + 1, f"{slowdown(data['mop4_dream_setassoc4_trhd500'][i]):.1f}%",
             ha='center', va='bottom', fontsize=8, color='black')

axs[0].set_ylabel('DREAM-C\nSlowdown (%)')
axs[0].set_xticks([i + 0.5*bar_width for i in index if not pd.isna(data['FILE'][i])])
axs[0].set_xticklabels([])
axs[0].set_xlim(-0.5, len(data))
axs[0].set_ylim(0, 50)
axs[0].grid(axis='y', linestyle='-', alpha=0.6, zorder=-10)
axs[0].get_xticklabels()[-1].set_weight('bold')
axs[0].legend(ncol=1, bbox_to_anchor=(0.84, 0.8), loc='center', prop={'weight': 'bold'}, borderpad=0.2, handletextpad=0.2, columnspacing=1.2, edgecolor='black', facecolor='white')

axs[1].bar([i + 0*bar_width for i in index], slowdown(data['mop4_dream_rand2_trhd250']), bar_width, label='TRH=250', edgecolor='black', color=colors[4], zorder=2)
axs[1].bar([i + 1*bar_width for i in index], slowdown(data['mop4_dream_rand4_trhd500']), bar_width, label='TRH=500', edgecolor='black', color=colors[3], zorder=2)
axs[1].bar([i + 2*bar_width for i in index], slowdown(data['mop4_dream_rand8_trhd1000']), bar_width, label='TRH=1000', edgecolor='black', color=colors[2], zorder=2)

axs[1].set_ylabel('DREAM-C\nSlowdown (%)')
axs[1].set_xticks([i + bar_width for i in index if not pd.isna(data['FILE'][i])])
axs[1].set_xticklabels(data.loc[~data['FILE'].isna()]['FILE'], rotation=48, ha='right')
axs[1].set_xlim(-0.5, len(data))
axs[1].set_ylim(0, 26)

# axs[1].axhline(y=1, color='black', linestyle='--', linewidth=1, zorder=1)
axs[1].set_yticks(list(range(0, 26, 5)))
axs[1].grid(axis='y', linestyle='-', alpha=0.6, zorder=-10)
axs[1].get_xticklabels()[-1].set_weight('bold')
axs[1].legend(ncol=1, bbox_to_anchor=(0.925, 0.7), loc='center', prop={'weight': 'bold'}, borderpad=0.2, handletextpad=0.2, columnspacing=1.2, edgecolor='black', facecolor='white')

plt.subplots_adjust(wspace=0, hspace=0.2)

plt.savefig('fig15.pdf', bbox_inches='tight', pad_inches=0.05)

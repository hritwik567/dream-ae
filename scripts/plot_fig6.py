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
data = pd.read_csv('stats/fig6.csv')

data = data.replace('MEAN', 'Average')

data['FILE'] = data['FILE'].apply(get_key)

data = sort_based_on_suite(data, ['spec2k17', 'stream4', 'gap6'])

data = insert_empty_rows(data, [11, 15, 21])

fig = plt.figure(figsize=(12, 2))
gs = gridspec.GridSpec(1, 1, height_ratios=[2])  # Adjust height_ratios as needed
axs = [None, None, None]
axs[0] = fig.add_subplot(gs[0])  # First subplot (smaller)

ymax = 9

# Plot bar graph
bar_width = 0.2
index = range(len(data))
colors = ['#5C4033', '#FBC740', '#2CA02C', '#FF7F0E', '#D62728']

axs[0].bar([i + 0*bar_width for i in index], 8 * data['mop4_para_drfmsb_eager'], bar_width, label='PARA (DRFMsb)', edgecolor='black', color=colors[0], zorder=2)
axs[0].bar([i + 1*bar_width for i in index], 8 * data['mop4_mint_drfmsb_eager'], bar_width, label='MINT (DRFMsb)', edgecolor='black', color=colors[2], zorder=2)
axs[0].bar([i + 2*bar_width for i in index], 8 * data['mop4_para_drfmsb_lazy'], bar_width, label='PARA (DREAM-R)', edgecolor='black', color=colors[1], zorder=2)
axs[0].bar([i + 3*bar_width for i in index], 8 * data['mop4_mint_drfmsb_lazy'], bar_width, label='MINT (DREAM-R)', edgecolor='black', color=colors[3], zorder=2)


axs[0].set_ylabel('RLP')
axs[0].set_xticks([i + bar_width for i in index if not pd.isna(data['FILE'][i])])
axs[0].set_xticklabels(data.loc[~data['FILE'].isna()]['FILE'], rotation=48, ha='right')
axs[0].set_xlim(-0.5, len(data))
axs[0].set_ylim(0, ymax+1)
axs[0].set_yticks(list(range(0, ymax+1, 2)))
axs[0].grid(axis='y', linestyle='-', alpha=0.6, zorder=-10)
axs[0].get_xticklabels()[-1].set_weight('bold')
axs[0].axhline(y=8, color='black', linestyle='--', linewidth=1, zorder=1)
axs[0].axhline(y=1, color='black', linestyle='--', linewidth=1, zorder=1)

axs[0].legend(ncol=5, bbox_to_anchor=(0.5, 0.91), loc='center', prop={'weight': 'bold'}, borderpad=0.2, handletextpad=0.2, columnspacing=1.2, edgecolor='none', facecolor='none')

plt.subplots_adjust(wspace=0, hspace=0.2)

plt.savefig('fig6.pdf', bbox_inches='tight', pad_inches=0.05)

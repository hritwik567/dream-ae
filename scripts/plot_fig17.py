import pandas as pd
import matplotlib.pyplot as plt
import matplotlib
from matplotlib import gridspec
matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42
plt.rcParams['font.family'] = 'serif'
plt.rcParams.update({'font.size': 8})

from common import get_key, insert_empty_rows, slowdown, sort_based_on_suite

mint = pd.read_csv('stats/fig8.csv')

data_mint = pd.DataFrame(columns=list(mint.columns) + ['TRH'])
data_mint = pd.concat([data_mint, mint.loc[mint['FILE'] == 'GEOMEAN']], ignore_index=True)
data_mint.loc[data_mint.index[-1], 'TRH'] = 500
data_mint = pd.concat([data_mint, mint1000.loc[mint1000['FILE'] == 'GEOMEAN']], ignore_index=True)
data_mint.loc[data_mint.index[-1], 'TRH'] = 1000
data_mint = pd.concat([data_mint, mint2000.loc[mint2000['FILE'] == 'GEOMEAN']], ignore_index=True)
data_mint.loc[data_mint.index[-1], 'TRH'] = 2000
data_mint = pd.concat([data_mint, mint4000.loc[mint4000['FILE'] == 'GEOMEAN']], ignore_index=True)
data_mint.loc[data_mint.index[-1], 'TRH'] = 4000

data_moat = pd.read_csv('stats/fig17.csv')
data_moat = pd.DataFrame(columns=['MOAT_slowdown', 'TRH'])
data_moat = pd.concat([data_moat, pd.DataFrame({'MOAT_slowdown': [prac.loc[prac['FILE'] == 'GEOMEAN', 'mop4_sb_moat_trhd500'].values[0]], 'TRH': [500]})], ignore_index=True)
data_moat = pd.concat([data_moat, pd.DataFrame({'MOAT_slowdown': [prac.loc[prac['FILE'] == 'GEOMEAN', 'mop4_sb_moat_trhd1000'].values[0]], 'TRH': [1000]})], ignore_index=True)
data_moat = pd.concat([data_moat, pd.DataFrame({'MOAT_slowdown': [prac.loc[prac['FILE'] == 'GEOMEAN', 'mop4_sb_moat_trhd2000'].values[0]], 'TRH': [2000]})], ignore_index=True)
data_moat = pd.concat([data_moat, pd.DataFrame({'MOAT_slowdown': [prac.loc[prac['FILE'] == 'GEOMEAN', 'mop4_sb_moat_trhd2000'].values[0]], 'TRH': [4000]})], ignore_index=True)

data_panopticon = pd.read_csv('stats/fig17.csv')
data_panopticon = pd.DataFrame(columns=['Panopticon_slowdown', 'TRH'])
data_panopticon = pd.concat([data_panopticon, pd.DataFrame({'Panopticon_slowdown': [prac.loc[prac['FILE'] == 'GEOMEAN', 'mop4_sb_panopticon500'].values[0]], 'TRH': [500]})], ignore_index=True)
data_panopticon = pd.concat([data_panopticon, pd.DataFrame({'Panopticon_slowdown': [prac.loc[prac['FILE'] == 'GEOMEAN', 'mop4_sb_panopticon1000'].values[0]], 'TRH': [1000]})], ignore_index=True)
data_panopticon = pd.concat([data_panopticon, pd.DataFrame({'Panopticon_slowdown': [prac.loc[prac['FILE'] == 'GEOMEAN', 'mop4_sb_panopticon2000'].values[0]], 'TRH': [2000]})], ignore_index=True)
data_panopticon = pd.concat([data_panopticon, pd.DataFrame({'Panopticon_slowdown': [prac.loc[prac['FILE'] == 'GEOMEAN', 'mop4_sb_panopticon4000'].values[0]], 'TRH': [4000]})], ignore_index=True)


dream_c = pd.read_csv('stats/fig15.csv')
data_dream = pd.DataFrame(columns=['dream_slowdown', 'TRH'])
data_dream = pd.concat([data_dream, pd.DataFrame({'dream_slowdown': [dream.loc[dream['FILE'] == 'GEOMEAN', 'mop4_dream_rand4_trhd500'].values[0]], 'TRH': [500]})], ignore_index=True)
data_dream = pd.concat([data_dream, pd.DataFrame({'dream_slowdown': [dream.loc[dream['FILE'] == 'GEOMEAN', 'mop4_dream_rand8_trhd1000'].values[0]], 'TRH': [1000]})], ignore_index=True)
data_dream = pd.concat([data_dream, pd.DataFrame({'dream_slowdown': [dream.loc[dream['FILE'] == 'GEOMEAN', 'mop4_dream_rand8_trhd2000'].values[0]], 'TRH': [2000]})], ignore_index=True)
data_dream = pd.concat([data_dream, pd.DataFrame({'dream_slowdown': [dream.loc[dream['FILE'] == 'GEOMEAN', 'mop4_dream_rand8_trhd4000'].values[0]], 'TRH': [4000]})], ignore_index=True)

print(data_dream)


fig = plt.figure(figsize=(4, 1.5))
gs = gridspec.GridSpec(1, 1, height_ratios=[2])  # Adjust height_ratios as needed
axs = [None, None, None]
axs[0] = fig.add_subplot(gs[0])  # First subplot (smaller)

patterns = [ "///" , "\\" , "|" , "-" , "+" , "x", "ooo", "O", ".", "*" ]

# Plot bar graph
bar_width = 0.4
index = range(0, 2 * len(data_para), 2)
colors = ['#5C4033', '#FBC740', '#2CA02C', '#FF7F0E', '#D62728', '#9467bd']
colors = ['#FBC740', '#2CA02C', '#FF7F0E', '#D62728', '#9467bd']

axs[0].bar([i + 0*bar_width for i in index], slowdown(data_moat['MOAT_slowdown']), bar_width, edgecolor='black', color=colors[0], zorder=2, label='MOAT')
axs[0].bar([i + 1*bar_width for i in index], slowdown(data_panopticon['Panopticon_slowdown']), bar_width, edgecolor='black', color=colors[1], zorder=2, label='Panopticon')
axs[0].bar([i + 2*bar_width for i in index], slowdown(data_mint['mop4_mint_drfmsb_lazy']), bar_width, edgecolor='black', color=colors[2], zorder=2, label='MINT (DREAM-R)')
axs[0].bar([i + 3*bar_width for i in index], slowdown(data_dream['dream_slowdown']), bar_width, edgecolor='black', color=colors[3], zorder=2, label='DREAM-C')

axs[0].axhline(y=slowdown(data_moat['MOAT_slowdown'][0]), color='black', linestyle='--', linewidth=1, zorder=1)
axs[0].text(index[-1] + 3*bar_width, slowdown(data_moat['MOAT_slowdown'])[0]+0.2, f"{slowdown(data_moat['MOAT_slowdown'][0]):.1f}%", ha='center', va='bottom', fontsize=7, color='black', weight='bold')


# ax2.text(1.5 + i*bar_width, y[i]/3, f"{data['sram'][i]}", ha='center', va='bottom', fontsize=8, color='black')
for i in range(len(index)):
    # axs[0].text(2*i + 0*bar_width, slowdown(data_prac['prac_mop4_sb'])[i]/3, f"{slowdown(data_prac['prac_mop4_sb'][i]):.1f}%", ha='center', va='bottom', fontsize=6, color='black')
    axs[0].text(2*i + 2.4*bar_width, slowdown(data_mint['mop4_mint_drfmsb_lazy'])[i]+0.2, f"{slowdown(data_mint['mop4_mint_drfmsb_lazy'][i]):.1f}%", ha='center', va='bottom', fontsize=7, color='black')
    axs[0].text(2*i + 3.4*bar_width, slowdown(data_dream['dream_slowdown'])[i]+0.2, f"{slowdown(data_dream['dream_slowdown'][i]):.1f}%", ha='center', va='bottom', fontsize=7, color='black')

axs[0].set_ylabel('Slowdown(%)')
# axs[0].set_xlabel('TRH')
axs[0].set_xticks([i + 1.5*bar_width for i in index if not pd.isna(data_para['TRH'][i/2])])
axs[0].set_xticklabels([f"TRH={i}" for i in data_para.loc[~data_para['TRH'].isna()]['TRH']], rotation=0, ha='center')
axs[0].set_xlim(-0.5, len(data_para)*2)
axs[0].set_ylim(0, 18)
axs[0].set_yticks([0, 5, 10])
axs[0].grid(axis='y', linestyle='-', alpha=0.6, zorder=-10)

axs[0].legend(ncol=2, bbox_to_anchor=(0.5, 0.85), loc='center', prop={'weight': 'bold'}, borderpad=0.2, handletextpad=0.2, columnspacing=0.8, edgecolor='none', facecolor='none')

plt.subplots_adjust(wspace=0, hspace=0.2)

plt.savefig('prac_comparison.pdf', bbox_inches='tight', pad_inches=0.05)
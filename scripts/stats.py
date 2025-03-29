import os
import sys
import pandas as pd
import re
import itertools
import numpy as np
import argparse
from scipy.stats.mstats import gmean

def parse_sim(file_path, args):
    data = {
        "File": get_workload_from_path(file_path),
        "TYPE": "memsim"
    }

    with open(file_path, 'r') as file:
        content = file.read()

        pattern = r"DRAM_RFM\s*:\s*(\d+)"
        match = re.search(pattern, content)
        if match:
            data["NUM_RFM"] = float(match.group(1))
    return data


suites = {
    'gap6': ['cc', 'pr', 'tc', 'bfs', 'bc', 'sssp'],
    'stream4': ['add', 'triad', 'copy', 'scale'],
    'spec2k17': ['blender_17', 'bwaves_17', 'cactuBSSN_17', 'cam4_17', 'deepsjeng_17', 'fotonik3d_17', 'gcc_17', 'imagick_17', 'lbm_17', 'leela_17', 'mcf_17', 'nab_17', 'namd_17', 'omnetpp_17', 'parest_17', 'perlbench_17', 'povray_17', 'roms_17', 'wrf_17', 'x264_17', 'xalancbmk_17', 'xz_17'],
    'mix': ['mix_1', 'mix_2', 'mix_3', 'mix_4', 'mix_5', 'mix_6', 'mix_7', 'mix_8', 'mix_9', 'mix_10']
}

def get_workload_from_path(file_path):
    all_workloads = list(itertools.chain.from_iterable(suites.values()))
    for workload in all_workloads:
        key = "_" + workload + "."
        if key in file_path:
            vals = file_path.split(key)
            typ = vals[0].split('/')[-1]
            name = workload
            return typ, name
    
    return None, None

def group_by_suite(df):
    # add a suite coulmn to the dataframe
    # and sort by file and suite
    df['SUITE'] = df['FILE'].apply(lambda x: next((k for k, v in suites.items() if x in v), None))
    df = df.sort_values(by=['SUITE', 'FILE'])
    # if suite is mix, sort by mix number
    if 'mix' in df['SUITE'].values:
        df['mix_num'] = df['FILE'].apply(lambda x: int(x.split('_')[-1]) if 'mix' in x else None)
        df = df.sort_values(by=['SUITE', 'mix_num'])
        df.drop(columns=['mix_num'], inplace=True)
    
    return df

def parse_dramsim3(file_path, args):
    # print("Parsing", file_path)
    typ, name =  get_workload_from_path(file_path)
    if not typ:
        return None
    # print("Typ:", typ, "Name:", name)
    data = {
        "FILE": name,
        "TYPE": typ
    }

    with open(file_path, 'r') as file:
        content = file.read()
        refab_match = re.findall(r"num_refab_cmds\s+=\s+(\d+)", content)
        refsb_match = re.findall(r"num_refsb_cmds\s+=\s+(\d+)", content)

        if refab_match:
            avg_refab = np.mean([int(r) for r in refab_match])
            data["AVG_REFAB"] = avg_refab

        if refsb_match:
            refab_match = [int(r) / 8 for r in refsb_match]
            avg_refab = np.mean(refab_match)
            data["AVG_REFAB"] = avg_refab

        drfmb_match = re.findall(r"num_drfmb_cmds\s+=\s+(\d+)", content)
        drfmsb_match = re.findall(r"num_drfmsb_cmds\s+=\s+(\d+)", content)
        drfmab_match = re.findall(r"num_drfmab_cmds\s+=\s+(\d+)", content)

        if drfmb_match:
            avg_drfm = np.mean([int(r) for r in drfmb_match])
        
        if drfmsb_match and avg_drfm == 0:
            avg_drfm = np.mean([int(r) for r in drfmsb_match])
        
        if drfmab_match and avg_drfm == 0:
            avg_drfm = np.mean([int(r) for r in drfmab_match])

        if drfmb_match or drfmsb_match or drfmab_match:
            data["AVG_DRFM_PER_REF"] = avg_drfm / avg_refab

        # mitig_used.x.x.x
        # mitig_wasted.x.x.x

        mitig_used_match = re.findall(r"mitig_used\.\d+\.\d+\.\d+\s*=\s*(\d+)", content)
        mitig_wasted_match = re.findall(r"mitig_wasted\.\d+\.\d+\.\d+\s*=\s*(\d+)", content)

        if mitig_used_match and mitig_wasted_match:
            total_used = sum([int(u) for u in mitig_used_match])
            total_wasted = sum([int(w) for w in mitig_wasted_match])
            total_mitig = total_used + total_wasted
            if total_mitig > 0:
                data["MITIG_UTIL"] = round(total_used / total_mitig, 6)
            else:
                data["MITIG_UTIL"] = 0
        total_burst = 0
        burst_match = re.findall(r"bursty_access_count\[32-\]\s*=\s*(\d+)", content)
        if burst_match:
            total_burst = np.mean([int(b) for b in burst_match])

        for i in range(32):
            burst_match = re.findall(r"bursty_access_count\[" + str(i) + "-" + str(i) + r"\]\s*=\s*(\d+)", content)
            if burst_match:
                total_burst += np.mean([int(b) for b in burst_match])

        total_read_writes = 0
        matches = re.findall(r"num_writes_done\s+=\s+(\d+)", content)
        if matches:
            total_read_writes = np.mean([int(m) for m in matches])

        matches = re.findall(r"num_reads_done\s+=\s+(\d+)", content)
        if matches:
            total_read_writes += np.mean([int(m) for m in matches])

        if total_burst > 0:
            data["TOTAL_READ_WRITES"] = total_read_writes
            data["AVG_BURST_LEN"] = float(total_read_writes) / float(total_burst)

        burst_match = re.findall(r"bursty_access_count\[31-31\]\s*=\s*(\d+)", content)
        if burst_match:
            data["AVG_BURST31"] = np.mean([int(b) for b in burst_match])

        acts_match = re.findall(r"num_act_cmds\s+=\s+(\d+)", content)
        total_acts = sum([int(m) for m in acts_match])

        if acts_match and refab_match and avg_refab > 0:
            acts_per_ref = np.mean([(int(a)/(32*int(r))) for a, r in zip(acts_match, refab_match)])
            data["ACTS_PER_REF"] = round(acts_per_ref, 3)
        

        matches = re.findall(r"num_write_drain\s+=\s+(\d+)", content)
        if matches and refab_match and avg_refab > 0:
            data["WRITE_DRAIN_PER_REF"] = np.mean([float(d)/float(r) for d, r in zip(matches, refab_match)])

        matches = re.findall(r"CORE_\d+_IPC\s*:\s*(\d+\.?\d*)", content)
        if matches:
            data["AVG_IPC"] = sum([float(m) for m in matches])/len(matches)

        matches = re.findall(r"CORE_\d+_MPKI\s*:\s*(\d+\.?\d*)", content)
        if matches:
            avg_mpki = sum([float(m) for m in matches])/len(matches)
            if args.filter_low_mpki and avg_mpki < 1:
                return None
            data["AVG_MPKI"] = avg_mpki

        match = re.search(r"AVG_CORE_CYCLES\s*:\s*(\d+)", content)
        if match:
            avg_core_cycles = float(match.group(1))

        matches = re.findall(r"\[\d\]\[\d+\]\s*mean:\s*(\d+\.?\d*)\s*stddev:\s*(\d+\.?\d*)\s*q50:\s*(\d+\.?\d*)\s*q90:\s*(\d+\.?\d*)\s*q99:\s*(\d+\.?\d*)\s*max:\s*(\d+\.?\d*)", content)
        if matches:
            avg = np.mean([float(m[0]) for m in matches])
            stddev = np.mean([float(m[1]) for m in matches])
            q99 = np.mean([float(m[4]) for m in matches])
            max_val = np.max([float(m[5]) for m in matches])

            data["TUSC.MEAN"] = avg
            data["TUSC.StdDev"] = stddev
            data["TUSC.Q99"] = q99
            data["TUSC.MAX"] = max_val

        acts_per_row = []
        for i in range(65):
            match = re.search(r"acts_per_row_per_trefw\[" + str(i) + "-" + str(i) + r"\]\s*=\s*(\d+)", content)
            if match:
                acts_per_row += [int(match.group(1))]
        
        match = re.search(r"acts_per_row_per_trefw\[65-\]\s*=\s*(\d+)", content)
        if match:
            acts_per_row += [int(match.group(1))]

        # add percentage of ACTs per row per tREFW
        total_acts_per_row = sum(acts_per_row)
        acts_per_row = [round((a * 100)/total_acts_per_row, 2) for a in acts_per_row]
        
        # for i, a in enumerate(acts_per_row):
        #     data["ACTS_PER_ROW_" + str(i)] = a

        matches1 = re.findall(r"CORE_\d+_TOTAL_ROB_STALLS\s*:\s*(\d+)", content)
        matches2 = re.findall(r"CORE_\d+_DRFM_ROB_STALLS\s*:\s*(\d+)", content)
        if matches1 and matches2:
            ratio = gmean([float(m1)/(float(m1) - float(m2)) for m1, m2 in zip(matches1, matches2)])
            data["TOTAL_ROB_STALLS"] = np.mean([int(x) for x in matches1])
            data["DRFM_ROB_STALL_PERCENT"] = round(ratio, 5)

        # average_read_latency           =      546.805   # Average read request latency (cycles)
        match = re.search(r"average_read_latency\s*=\s*(\d+\.?\d*)", content)
        if match:
            data["AVG_READ_LATENCY"] = float(match.group(1))

        matches = re.findall(r"CORE_\d+_INST\s*:\s*(\d+)", content)
        if matches:
            inst = sum([float(m) for m in matches])
            data["APKI"] = (sum([int(x) for x in acts_match]) * 1000)/inst
            if args.filter_low_apki and data["APKI"] < 1:
                return None
            if args.filter_low_apki and "blender" in file_path:
                return None
        matches = re.findall(r"OS_PAGE_MISS\s*:\s*(\d+)", content)
        total_pages = re.findall(r"Initialized OS for (\d+) pages", content)
        if matches and total_pages:
            data["mem_util"] = (sum([float(m) for m in matches]) / float(total_pages[0])) * 100
        
        # average_bandwidth
        matches = re.findall(r"average_bandwidth\s*=\s*(\d+\.?\d*)", content)
        num_cycles = re.findall(r"num_cycles\s*=\s*(\d+)", content)
        num_refsb_cmds = re.findall(r"num_refsb_cmds\s*=\s*(\d+)", content)
        if matches and num_cycles and num_refsb_cmds:
            # max_bw = (3 * (64 / 8) * 1e9)/(1024**3)
            max_bw = (3 * (64 / 8))
            # avg_bw = np.mean([(float(m) * float(c))/(float(c) - (float(r)/4)*660) for m, c, r in zip(matches, num_cycles, num_refsb_cmds)])
            avg_bw = np.mean([float(m) for m in matches])
            data["BW_UTIL"] = round((avg_bw/ max_bw) * 100, 2)

    return data

def parse_directory(directory_path, args):
    all_data = []

    for entry in os.listdir(directory_path):
        entry_path = os.path.join(directory_path, entry)

        if os.path.isdir(entry_path):
            all_data.extend(parse_directory(entry_path, args))
        elif "sim_" in entry_path:
            file_data = parse_sim(entry_path, args)
            if file_data:
                all_data.append(file_data)
        else:
            file_data = parse_dramsim3(entry_path, args)
            if file_data:
                all_data.append(file_data)
    return all_data



def pivot(df, column):
    print("Coulmns:", df.columns)
    print("Pivoting on", column)
    pivot_table = df.pivot_table(index='FILE', columns='TYPE', values=column)

    # Reset index to flatten the DataFrame
    pivot_table = pivot_table.reset_index()

    return pivot_table

def add_best(df):
    # add best IPC column, max of all the columns except file
    columns = df.columns
    columns = columns.drop(['FILE', 'SUITE'])

    df['best'] = df[columns].max(axis=1)
    return df

def add_slowdown(df, baseline):
    # compare evey column excpet file to best and update the value of each column
    columns = df.columns
    columns = columns.drop(['FILE', 'SUITE', baseline])

    for c in columns:
        df[c] = round(df[c] / df[baseline], 4)

    df[baseline] = round(df[baseline] / df[baseline], 4)

    return df

def add_mean(df, dropna=False):
    mean_row = {"FILE": "MEAN"}

    for c in df.columns:
        if c not in ['FILE', 'SUITE']:
            # print(df[c].dropna())
            mean_row[c] = round(df[c].dropna().mean(), 6)

    # Convert the geomean_row to a DataFrame and concatenate
    mean_df = pd.DataFrame([mean_row], columns=df.columns)
    df = pd.concat([df, mean_df], ignore_index=True)

    return df

def add_geomean(df, dropna=False):
    geomean_row = {"FILE": "GEOMEAN"}

    for c in df.columns:
        if c not in ['FILE', 'SUITE']:
            geomean_row[c] = gmean(df[c].dropna())

    # Convert the geomean_row to a DataFrame and concatenate
    geomean_df = pd.DataFrame([geomean_row], columns=df.columns)
    df = pd.concat([df, geomean_df], ignore_index=True)

    return df

def add_geomean_suite(df, dropna=False):
    geomean_dfs = []
    
    # get df with no suite
    nosuite_df = df[df['SUITE'].isnull()]

    for suite in suites.keys():
        suite_df = df[df['SUITE'] == suite]
        geomean_row = {"FILE": suite + "_GEOMEAN"}

        for c in suite_df.columns:
            if c not in ['FILE', 'SUITE']:
                geomean_row[c] = gmean(suite_df[c].dropna())

        geomean_df = pd.DataFrame([geomean_row], columns=df.columns)
        new_df = pd.concat([suite_df, geomean_df], ignore_index=True)
        geomean_dfs.append(new_df)


    return pd.concat(geomean_dfs + [nosuite_df], ignore_index=True)

def main(args):
    data = []
    for directory in args.directory_path:
        if not os.path.isdir(directory):
            print("Invalid directory path")
        data += parse_directory(directory, args)
    
    df = pd.DataFrame(data)


    if args.type:
        df = df[df['TYPE'] == args.type]
    else:
        df = pivot(df, args.pivot)


    df = group_by_suite(df)

    if args.ignorecols:
        df = df.drop(columns=args.ignorecols)
    
    if args.cols:
        # add FILE column to the list of columns
        args.cols = ['FILE'] + args.cols
        df = df[args.cols]

    if args.dropna:
        df = df.dropna()

    if not args.noslowdown and args.baseline == "best":
        df = add_best(df)

    if args.mean:
        df = add_mean(df)
    elif args.gmean:
        df = add_geomean(df)
    
    if args.gmean_suite:
        df = add_geomean_suite(df)

    if not args.noslowdown:
        df = add_slowdown(df, args.baseline)

    if args.suite:
        df = df[df['SUITE'] == args.suite]

    if args.out:
        df.to_csv(args.out, index=False)
    else:
        print(df.to_string())
       
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Parse stats files')
    # multiple directories can be passed
    parser.add_argument('directory_path', type=str, nargs='+', help='Path to the directory containing stats files')
    parser.add_argument('-pivot', type=str, default="AVG_IPC", help='Column to pivot on')
    parser.add_argument('-mean', action='store_true', help='Add mean row')
    parser.add_argument('-gmean', action='store_true', help='Add geomean row')
    parser.add_argument('-dropna', action='store_true', help='Drop rows with Nan')
    parser.add_argument('-ignorecols', nargs='+', help='Columns to ignore')
    parser.add_argument('-cols', nargs='+', help='Columns to Select')
    parser.add_argument('-noslowdown', action='store_true', help='Do not calculate slowdown')
    parser.add_argument('-baseline', type=str, default='best', help='Baseline column for slowdown')
    parser.add_argument('-type', type=str, help='Type of workload to select')
    parser.add_argument('-suite', type=str, help='Suite to select')
    parser.add_argument('-gmean_suite', action='store_true', help='Add geomean row for each suite')
    parser.add_argument('-filter_low_mpki', action='store_true', help='Filter workloads with MPKI < 1')
    parser.add_argument('-filter_low_apki', action='store_true', help='Filter workloads with APKI < 1')
    parser.add_argument('-out', type=str, help='Output file')
    args = parser.parse_args()
    main(args)


import pandas as pd


def get_key(val):
    val = val.replace('_17', '')
    return val

def insert_empty_rows(df, positions):
    empty_row = pd.Series([None] * len(df.columns), index=df.columns)
    for pos in sorted(positions, reverse=True):
        df = pd.concat([df.iloc[:pos+1], pd.DataFrame([empty_row]), df.iloc[pos+1:]]).reset_index(drop=True)
    return df

# (1/x-1)*100 for each value 
def slowdown(speedup):
    return (1/speedup - 1) * 100

def add_mean(df, dropna=False):
    mean_row = {"FILE": "MEAN"}

    for c in df.columns:
        if c not in ['FILE', 'SUITE']:
            mean_row[c] = round(df[c].dropna().mean(), 3)

    # Convert the geomean_row to a DataFrame and concatenate
    mean_df = pd.DataFrame([mean_row], columns=df.columns)
    df = pd.concat([df, mean_df], ignore_index=True)

    return df

def sort_based_on_suite(df, suite_order):
    df['SUITE'] = pd.Categorical(df['SUITE'], suite_order)
    df = df.sort_values(['SUITE', 'FILE']).reset_index(drop=True)
    return df

def suite_map(suite):
    if suite == 'spec2k17':
        return "SPEC2017"
    elif suite == 'gap6':
        return "GAP"
    elif suite == 'stream4':
        return "STREAM"
    else:
        return suite 

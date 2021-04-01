import os
from matplotlib import pyplot as plt

block_sizes = [8, 32, 64]
assocs = [1, 4, 8, 0]
write_policies = ["back_alloc", "back_noAlloc", "through_alloc", "through_noAlloc"]
replace_policies = ['binTree', 'LRU', 'PLRU']

default_rp = replace_policies[0]
default_write = write_policies[0]
default_bs = block_sizes[0]
default_assocs = 8

in_dir = "output/stats"
out_dir = "stats"


def get_filename(blocksize, ways, replace, write):
    return f"stats_{blocksize}_{ways}_{replace}_{write}.tsv"


def get_avg(lis):
    return sum(lis) / len(lis)


def read_stats(blocksize, ways, replace, write):
    filename = get_filename(blocksize, ways, replace, write)
    filename = os.path.join(in_dir, filename)
    lines = []
    with open(filename) as f:
        lines = f.readlines()
    lines = [x.strip().split('\t') for x in lines]
    data = []
    for line in lines[1:]:
        d = {}
        for i in range(len(lines[0])):
            d[lines[0][i]] = float(line[i])
        data.append(d)
    return data


def get_miss_rate(stats):
    row_data = []
    for i in range(4):
        mr = stats[i]['miss rate']
        row_data.append(mr)
    row_data.append(f'{get_avg(row_data):.1f}')
    return row_data


def get_space(stats):
    row_data = []
    cache_list = []
    replace_list = []
    for i in range(4):
        cache = int(stats[i]['cache space'])
        replace = int(stats[i]['replace space'])
        cache_list.append(cache)
        replace_list.append(replace)
        
        row_data.append(f'{cache}+{replace}')
    cache_avg = int(get_avg(cache_list))
    replace_avg = int(get_avg(replace_list))
    row_data.append(f'{cache_avg}+{replace_avg}')
    return row_data
    

def write_file(out_file, row_labels, mat, md=True):
    out_file = os.path.join(out_dir, out_file)
    maxlen_label = max(list(map(len, row_labels))) + 1
    smat = []
    for row in mat:
        smat.append([f'{num}' for num in row])
    nrows = len(mat)
    ncols = len(mat[0])
    print('writing to', out_file)
    with open(out_file, 'w') as f:
        for r, label in enumerate(row_labels):
            if md:
                # Markdown
                maxlen = 0
                for c in range(ncols):
                    maxlen = max(len(smat[i][c]) for i in range(nrows))
                f.write('| ')
                f.write(label)
                f.write(' ' * (maxlen_label - len(label)))
                for i in range(ncols):
                    pad = maxlen - len(smat[r][i])
                    smat[r][i] += pad * ' '
                line = '| '
                line += ' | '.join(smat[r])
                line += ' |'
                
                f.write(line)
                f.write('\n')
                plt.plot(mat[r], label=label)
            else:
                f.write(label + '\t');
                f.write('\t'.join(smat[r]))
                f.write('\n')



def process_structure(out_file, getter, calc_avg=False):
    mat = []
    row_labels = []
    for b in block_sizes:
        for w in assocs:
            params = (b, w, default_rp, default_write)
            stats = read_stats(*params)
            row_data = getter(stats)
            mat.append(row_data)
            row_labels.append(f'{b}, {w}')

    write_file(out_file, row_labels, mat)


def process_replace(out_file, getter, calc_avg=False):
    mat = []
    row_labels = []
    for rp in replace_policies:
        params = (8, 8, rp, default_write)
        stats = read_stats(*params)
        row_data = getter(stats)
        mat.append(row_data)
        row_labels.append(rp)
    
    write_file(out_file, row_labels, mat)


def process_write(out_file, getter, calc_avg=False):
    mat = []
    row_labels = []
    for wp in write_policies:
        params = (default_bs, default_assocs, default_rp, wp)
        stats = read_stats(*params)
        row_data = getter(stats)
        mat.append(row_data)
        row_labels.append(wp)

    write_file(out_file, row_labels, mat)


def process_stats():
    getters = [get_miss_rate, get_space]
    suffices = ["mr", "space"]
    get_avg = [True, False]
    for i, getter in enumerate(getters):
        suf = suffices[i]
        process_structure(f"structure_{suf}.txt", getter)
        process_replace(f"replace_{suf}.txt", getter)
        process_write(f"write_{suf}.txt", getter)


if __name__ == '__main__':
    process_stats()

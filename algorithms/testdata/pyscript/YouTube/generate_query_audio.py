import numpy as np
import struct
import random
from collections import defaultdict


def read_fbin(filename, start_idx=0, chunk_size=None):
    """ Read *.fbin file that contains float32 vectors
    Args:
        :param filename (str): path to *.fbin file
        :param start_idx (int): start reading vectors from this index
        :param chunk_size (int): number of vectors to read.
                                 If None, read all vectors
    Returns:
        Array of float32 vectors (numpy.ndarray)
    """
    with open(filename, "rb") as f:
        nvecs, dim = np.fromfile(f, count=2, dtype=np.int32)
        print(nvecs)
        print(dim)
        nvecs = (nvecs - start_idx) if chunk_size is None else chunk_size
        arr = np.fromfile(f, count=nvecs * dim, dtype=np.float32,
                          offset=start_idx * 4 * dim)
    return arr.reshape(nvecs, dim).tolist()


def read_meta(file_path):
    with open(file_path, 'r', encoding='utf-8') as file:
        content = file.read().splitlines()
        meta_type = content[1]
        content.pop(0)
        content.pop(0)
    return meta_type, content


def write_query(data_lst, filename, label_list):
    """
    Saves the meta list to a .txt file with elements separated by spaces.

    :param label_list:
    :param data_lst:
    :param filename: Path to the output .txt file.
    """
    nm = [len(data_lst), len(data_lst[0])]

    with open(filename, 'w', encoding='utf-8') as f:
        f.write(' '.join(map(str, nm)))
        for item in data_lst:
            f.write('\n')
            f.write(' '.join(map(str, item)))
            f.write(' ')
            cond = f'label="{random.choice(label_list)}"'
            f.write(cond)


if __name__ == "__main__":
    data_list = read_fbin("res/YouTube-audio/YouTube.bin")
    random_sample = random.sample(data_list, 100)
    query_vector = [list(item) for item in random_sample]
    data_list.clear()
    meta_type, content = read_meta("res/YouTube-audio/meta.txt")
    label_dict = dict()
    for line in content:
        if line not in label_dict:
            label_dict[line] = 1
        else:
            label_dict[line] += 1
    labels = []
    for key in label_dict.keys():
        if label_dict[key] > 3000:
            labels.append(key)
    labels = list(set(labels))
    write_query(query_vector, "res/YouTube-audio/query.txt", labels)

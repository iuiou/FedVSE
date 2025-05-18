import numpy as np
import struct
import random
from collections import defaultdict


class VectorDataType:
    def __init__(self, vid, data):
        self.vid = vid
        self.data = data


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
            cond1 = f'color="{random.choice(label_list)}"'
            f.write(cond1)
            f.write(' ')
            a = b = 0
            while True:
                a = random.randint(0, 10000)
                b = random.randint(0, 10000)
                if abs(a - b) > 100:
                    break
            _a_ = min(a, b)
            _b_ = max(a, b)
            cond2 = f'{_a_}<=value<={_b_}'
            f.write(cond2)


def read_vector_data_from_fivecs(file_name, data_list):
    with open(file_name, 'rb') as file:
        # Read the number of vectors and dimension
        nvecs, = struct.unpack('i', file.read(4))
        dim, = struct.unpack('i', file.read(4))
        print(f"Read data: size = {nvecs}, dimension = {dim}")

        data_list.clear()

        for _ in range(nvecs):
            # Read the vector ID
            vid, = struct.unpack('i', file.read(4))
            # Read the vector data
            vec = struct.unpack(f'{dim}f', file.read(dim * 4))
            if len(vec) != dim:
                raise RuntimeError("Error reading file")

            data_list.append(VectorDataType(vid, vec))


if __name__ == "__main__":
    query_vector = []
    for i in range(5):
        data_list = []
        read_vector_data_from_fivecs(f"res/deep_{i}.fivecs", data_list)
        random_sample = random.sample(data_list, 20)
        query_tmp = [list(item.data) for item in random_sample]
        query_vector.extend(query_tmp)
    labels = ["red", "orange", "yellow", "green", "blue", "cyan", "purple", "write", "black", "gold", "silver", "pink",
              "brown"]
    write_query(query_vector, "res/query.txt", labels)

import os
import argparse
import numpy as np
import pandas as pd
import struct
from collections import defaultdict

off = 253596 + 252451 + 253044
qqq = 4


class VectorDataType:
    def __init__(self, vid, data):
        self.vid = vid
        self.data = data


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

            vector_data = VectorDataType(dim, vid)
            vector_data.data = list(vec)
            data_list.append(vector_data)


def dump_vector_data_to_fivecs(file_name, data_list):
    with open(file_name, 'wb') as file:
        # Write the number of vectors and dimension
        nvecs = len(data_list)
        dim = len(data_list[0].data)
        file.write(struct.pack('i', nvecs))
        file.write(struct.pack('i', dim))
        print(f"Write data: size = {nvecs}, dimension = {dim}")

        for vector_data in data_list:
            # Write the vector ID
            file.write(struct.pack('i', vector_data.vid))
            # Write the vector data
            file.write(struct.pack(f'{dim}f', *vector_data.data))


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


def write_fbin(filename, vectors):
    """ Write float32 vectors to a *.fbin file
    Args:
        :param filename (str): path to *.fbin file
        :param vectors (numpy.ndarray): array of float32 vectors to write
    """
    nvecs, dim = vectors.shape
    with open(filename, "wb") as f:
        np.array([nvecs, dim], dtype=np.int32).tofile(f)
        vectors.astype(np.float32).tofile(f)


def read_meta(file_path):
    with open(file_path, 'r', encoding='utf-8') as file:
        content = file.read().splitlines()
        meta_type = content[1]
        content.pop(0)
        content.pop(0)
    return meta_type, content


def write_meta(metadata, filename, msg):
    """
    Saves the meta list to a .txt file with elements separated by spaces.

    :param meta: List of integers representing image sizes.
    :param filename: Path to the output .txt file.
    """
    nm = [len(metadata), len(msg) // 2]

    with open(filename, 'w', encoding='utf-8') as f:
        f.write(' '.join(map(str, nm)))
        f.write('\n')
        f.write(' '.join(map(str, msg)))
        f.write('\n')
        for m in metadata:
            m = m.replace(' ', '')
            f.write(m)
            f.write('\n')


def dirichlet_label_imbalance(n_classes, n_parties, beta, classes):
    proportions = np.random.dirichlet([beta] * n_parties, size=n_classes)
    allocation = np.zeros((n_classes, n_parties), dtype=int)
    for j in range(n_classes):
        for k in range(n_parties):
            allocation[j][k] = int(proportions[j][k] * classes[j])

    # 确保每个类别的数据总数不变
    for j in range(n_classes):
        total = sum(allocation[j])
        if total < classes[j]:
            diff = classes[j] - total
            for k in range(n_parties):
                if diff == 0:
                    break
                allocation[j][k] += 1
                diff -= 1

    return allocation


def _non_IID_(meta, vectors, num_silos, beta):
    # Step 1: 统计标签出现次数，并将 vectors 划分为子集
    label_count = defaultdict(list)
    for i, label in enumerate(meta):
        label_count[label].append(vectors[i])

    classes = [len(label_count[label]) for label in label_count]
    print(f'classes: {classes}')
    n_classes = len(classes)

    # Step 2: 使用 dirichlet 函数分配数据
    allocation = dirichlet_label_imbalance(n_classes, num_silos, beta, classes)

    # Step 3: 保存得到的 dirichlet 函数矩阵
    np.savetxt(f"dirichlet_allocation_matrix_{qqq}.txt", allocation, fmt="%d")
    print("Dirichlet Allocation Matrix:")
    print(allocation)

    # Step 4: 将各个子集中对应数量的数据划分给各个 silo 端
    silos = [[] for _ in range(num_silos)]
    metas = [[] for _ in range(num_silos)]
    labels = list(label_count.keys())

    for i, label in enumerate(labels):
        vectors_for_label = label_count[label]
        start_index = 0
        for j in range(num_silos):
            end_index = start_index + allocation[i][j]
            metas[j].extend([label for _ in range(allocation[i][j])])
            silos[j].extend(vectors_for_label[start_index:end_index])
            start_index = end_index
    return silos, metas


if __name__ == "__main__":
    # parser = argparse.ArgumentParser(description="parameter setting")
    # parser.add_argument("--vectors", type=str, nargs="?", help="vector data set")
    # parser.add_argument("--meta", type=str, nargs="?", help="meta data set")
    # parser.add_argument("--output", type=str, nargs="?", default="res", help="output directory")
    # parser.add_argument("-silo", type=int, nargs="?", default=30, help="number of silo")
    # parser.add_argument("-IID", type=int, nargs="?", default=0, help="type of IID(0: IID, 1: non-IID-1, 2: non-IID-2)")

    # args = parser.parse_args()
    path = "res"
    vectors_name = f"YouTube_{qqq}"
    meta_name = f"meta_{qqq}"
    num_silo = 20
    beta = 0.5
    msg, meta = read_meta(f'{path}/YouTube-rgb/{meta_name}.txt')
    vectors = read_fbin(f'{path}/YouTube-rgb/{vectors_name}.bin')
    points = []
    for i in range(len(vectors)):
        points.append(VectorDataType(i+off, vectors[i]))
    silos, metas = _non_IID_(meta, points, num_silo, beta)
    for i in range(num_silo):
        dump_vector_data_to_fivecs(f'{path}/YouTube-rgb/{vectors_name}-{i}.fivecs', silos[i])
        write_meta(metas[i], f'{path}/YouTube-rgb/{meta_name}-{i}.txt', msg.split(' '))
        print(f'{i} done')

import os
import argparse
import numpy as np
import pandas as pd
import struct
import random
from collections import defaultdict


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


def _non_IID_(meta, vectors, num_silos, silo_labels):
    # Step 1: Group vectors by their labels
    label_to_vectors = defaultdict(list)
    for i, label in enumerate(meta):
        label_to_vectors[label].append(vectors[i])

    # Step 3: Distribute the vectors among the silos based on the labels
    silos = [[] for _ in range(num_silos)]
    metas = [[] for _ in range(num_silos)]

    clo_key = dict()

    for key in silo_labels.keys():
        for label in silo_labels[key]:
            if label not in clo_key:
                clo_key[label] = 1
            else:
                clo_key[label] += 1
    print(clo_key)

    clo_id = {key: 0 for key in clo_key.keys()}
    clo_remainder = {key: len(label_to_vectors[key]) % clo_key[key] for key in clo_key.keys()}
    for i in range(num_silos):
        lab = silo_labels[i]
        for label in lab:
            start = clo_id[label]
            vectors_per_silo = len(label_to_vectors[label]) // clo_key[label]
            if clo_remainder[label] > 0:
                vectors_per_silo += 1
                clo_remainder[label] -= 1
            end = start + vectors_per_silo
            silos[i].extend(label_to_vectors[label][start:end])
            metas[i].extend([label for _ in range(vectors_per_silo)])
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
    vectors_name = "deep"
    meta_name = "meta"
    num_silo = 5
    silo_labels = {}
    for no in range(10):
        points = []
        read_vector_data_from_fivecs(f"res/base.1B.fbin_{no}.fivecs", points)
        msg, meta = read_meta(f"res/meta_{no}.txt")
        if no == 0:
            unique_labels = list(set(meta))
            silo_labels = {i: random.sample(unique_labels, min(5, len(unique_labels))) for i in range(num_silo)}
        silos, metas = _non_IID_(meta, points, num_silo, silo_labels)
        for i in range(num_silo):
            dump_vector_data_to_fivecs(f'{path}/{vectors_name}_{i}_{no}.fivecs', silos[i])
            write_meta(metas[i], f'{path}/{meta_name}_{i}_{no}.txt', msg.split(' '))
            print(f'{i} done')

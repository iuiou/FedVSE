import os
import argparse
import numpy as np
import pandas as pd
import struct


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


def _IID_(vectors, meta, num_silos):
    """
    将vector与meta分成尽量均匀的silo份

    Args:
        vector (list or np.ndarray): 向量数据
        meta (list or np.ndarray): 元数据
        num_silos (int): silo的数量

    Returns:
        list: 包含每个silo的vector和meta的列表
    """
    # 确保vector和meta的长度相同
    if len(vectors) != len(meta):
        raise ValueError("vector和meta的长度必须相同")

    # 计算每个silo的大小
    total_length = len(vectors)
    silo_size = total_length // num_silos
    remainder = total_length % num_silos

    # 初始化silo列表
    silos = []

    # 分配数据到每个silo
    start_idx = 0
    for i in range(num_silos):
        # 计算当前silo的结束索引
        end_idx = start_idx + silo_size + (1 if i < remainder else 0)

        # 分配vector和meta数据到当前silo
        silo_vector = vectors[start_idx:end_idx]
        silo_meta = meta[start_idx:end_idx]

        # 添加到silo列表
        silos.append((silo_vector, silo_meta))

        # 更新起始索引
        start_idx = end_idx

    return silos


if __name__ == "__main__":
    # parser = argparse.ArgumentParser(description="parameter setting")
    # parser.add_argument("--vectors", type=str, nargs="?", help="vector data set")
    # parser.add_argument("--meta", type=str, nargs="?", help="meta data set")
    # parser.add_argument("--output", type=str, nargs="?", default="res", help="output directory")
    # parser.add_argument("-silo", type=int, nargs="?", default=30, help="number of silo")
    # parser.add_argument("-IID", type=int, nargs="?", default=0, help="type of IID(0: IID, 1: non-IID-1, 2: non-IID-2)")

    # args = parser.parse_args()
    path = "res"
    vectors_name = "WIT"
    meta_name = "meta"
    num_silo = 5
    msg, meta = read_meta(f'{path}/{meta_name}.txt')
    vectors = read_fbin(f'{path}/{vectors_name}.bin')
    points = []
    for i in range(len(vectors)):
        points.append(VectorDataType(i, vectors[i]))
    silos = _IID_(points, meta, num_silo)
    for i in range(num_silo):
        dump_vector_data_to_fivecs(f'{path}/{vectors_name}_{i}.fivecs', silos[i][0])
        write_meta(silos[i][1], f'{path}/{meta_name}_{i}.txt', msg.split(' '))

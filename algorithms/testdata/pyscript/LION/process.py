import numpy as np
import pyarrow.parquet as pq
import pandas as pd


def read_npy(filename):
    data = np.load(filename)
    return data


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


def read_parquet(filename):
    table = pq.read_table(filename)
    df = table.to_pandas()
    return df


def save_meta_to_txt(metadata, filename, msg):
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
            f.write(' '.join(map(str, m)))
            f.write('\n')


if __name__ == '__main__':
    # data = read_npy('data/text_emb_0.npy')
    # write_fbin('res/LION.bin', data)
    df = read_parquet('data/metadata_0.parquet')
    height_column = df['original_height']
    height = [value for value in height_column]
    key_column = df['key']
    key = [value for value in key_column]
    meta = list()
    msg = ['key', 'int', 'height', 'int']
    for i in range(len(height)):
        meta.append((key[i], height[i]))
    save_meta_to_txt(meta, 'res/meta.txt', msg)

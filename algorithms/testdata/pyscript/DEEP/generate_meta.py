import numpy as np
import struct
import random

nvecs = 0
dim = 0


class VectorDataType:
    def __init__(self, vid, data):
        self.vid = vid
        self.data = data


def read_vector_data_from_fivecs(file_name, data_list):
    global nvecs, dim
    with open(file_name, 'rb') as file:
        # Read the number of vectors and dimension
        nvecs, = struct.unpack('i', file.read(4))
        dim, = struct.unpack('i', file.read(4))


def read_fbin(filename, start_idx=0, chunk_size=None):
    global nvecs, dim
    with open(filename, "rb") as f:
        nvecs, dim = np.fromfile(f, count=2, dtype=np.int32)


def generate_meta(filename, msg, labels: list):
    global nvecs
    nm = [nvecs, len(msg) // 2]

    with open(filename, 'w', encoding='utf-8') as f:
        f.write(' '.join(map(str, nm)))
        f.write('\n')
        f.write(' '.join(map(str, msg)))
        f.write('\n')
        for _ in range(nvecs):
            w = random.choice(labels)
            f.write(w)
            f.write('\n')


if __name__ == '__main__':
    for no in range(10):
        data_list = []
        read_vector_data_from_fivecs(f"res/base.1B.fbin_{no}.fivecs", data_list)
        colors = ["red", "orange", "yellow", "green", "blue", "cyan", "purple", "write", "black", "gold", "silver", "pink",
                  "brown"]
        msg = ["color", "string"]
        generate_meta(f"res/meta_{no}.txt", msg, colors)

import numpy as np
import struct


class VectorDataType:
    def __init__(self, vid, data):
        self.vid = vid
        self.data = data


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


if __name__ == '__main__':
    vecs = []
    meta = []
    for j in range(20):
        for i in range(1, 5):
            ans = []
            read_vector_data_from_fivecs(f'res/YouTube-rgb/YouTube_{i}-{j}.fivecs', ans)
            print(f'Read {len(ans)} vectors from YouTube_{i}-{j}.fivecs')
            msg, res = read_meta(f'res/YouTube-rgb/meta_{i}-{j}.txt')
            vecs.extend(ans)
            meta.extend(res)
        output_fbin = f"res/YouTube-rgb/YouTube_{j}.fivecs"
        dump_vector_data_to_fivecs(output_fbin, vecs)
        output_txt = f"res/YouTube-rgb/meta_{j}.txt"
        msg = ['label', 'string']
        write_meta(meta, output_txt, msg)
        vecs.clear()
        meta.clear()

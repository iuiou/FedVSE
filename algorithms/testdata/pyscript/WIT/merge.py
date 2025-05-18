import numpy as np


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


def read_file_to_array(file_path):
    with open(file_path, 'r', encoding='utf-8') as file:
        content = file.read().splitlines()
    return content


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
            m = m.replace(' ', '')
            f.write(m)
            f.write('\n')


if __name__ == '__main__':
    vecs = []
    meta = []
    for i in range(10):
        filename = f"res/WIT_{i}.bin"
        vectors = read_fbin(filename)
        vecs.extend(vectors)
    print(len(vecs))
    vecs_array = np.array(vecs, dtype=np.float32)
    output_fbin = f"res/WIT.bin"
    write_fbin(output_fbin, vecs_array)
    for i in range(10):
        filename = f"res/meta_{i}.txt"
        cont = read_file_to_array(filename)
        meta.extend(cont)
    output_txt = f"res/meta.txt"
    msg = ['size', 'int']
    save_meta_to_txt(meta, output_txt, msg)


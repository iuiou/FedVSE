import numpy as np
import struct


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
    return arr.reshape(nvecs, dim)


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

            data_list.append(VectorDataType(vid,vec))


def read_ground_truth_from_ivecs(file_name, answer_list):
    try:
        with open(file_name, 'rb') as file:
            answer_list.clear()
            while True:
                # Read the dimension
                dim_bytes = file.read(4)
                if not dim_bytes:
                    break  # End of file reached

                dim, = struct.unpack('i', dim_bytes)

                # Read the vector data
                vec_bytes = file.read(dim * 4)
                if len(vec_bytes) != dim * 4:
                    raise RuntimeError("Error reading file")

                vec = struct.unpack(f'{dim}i', vec_bytes)
                answer_list.append(list(vec))
    except FileNotFoundError:
        print(f"Failed to open file for reading ground truth: {file_name}")
    except RuntimeError as e:
        print(e)


# Example usage
answer_list = []
read_ground_truth_from_ivecs("res/YouTube-audio/gt_128_5.ivecs", answer_list)
print(answer_list)


def read_meta(file_path):
    with open(file_path, 'r', encoding='utf-8') as file:
        content = file.read().splitlines()
    return content


if __name__ == "__main__":
    answer_list = []
    read_ground_truth_from_ivecs("res/YouTube-audio/YouTube_0.fivecs", answer_list)
    print(answer_list)

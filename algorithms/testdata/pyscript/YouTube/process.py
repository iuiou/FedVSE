import os
import tensorflow as tf
import pandas as pd
import numpy as np

meta = []
vecs = []

feature_description = {
    "id": tf.io.FixedLenFeature([], tf.string),  # Video ID as a string
    "labels": tf.io.VarLenFeature(tf.int64),  # Labels as a sparse list of integers
    "mean_rgb": tf.io.FixedLenFeature([1024], tf.float32),  # 1024 float features
    "mean_audio": tf.io.FixedLenFeature([128], tf.float32),  # 128 float features
}


def read_csv(file_path):
    df = pd.read_csv(file_path)
    return df


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


def read_tfrecord(tfrecord_file):
    dataset = tf.data.TFRecordDataset(tfrecord_file)
    parsed_dataset = []
    for example in dataset:
        parsed_example = parse_example(example)
        parsed_dataset.append(parsed_example)
    return parsed_dataset


def parse_example(example_proto):
    parsed_features = tf.io.parse_single_example(example_proto, feature_description)
    labels = tf.sparse.to_dense(parsed_features["labels"], default_value=0)

    return {
        "id": parsed_features["id"],
        "labels": labels,
        "mean_rgb": parsed_features["mean_rgb"],
        "mean_audio": parsed_features["mean_audio"]
    }


def solve_tfrecord(parsed_dataset, _LABEL):
    global meta, vecs
    for parsed_record in parsed_dataset:  # Display the first 5 records
        labels = parsed_record['labels'].numpy().tolist()
        lab_no = labels[0]
        meta.append(_LABEL[lab_no])
        rgb = parsed_record['mean_rgb'].numpy().tolist()
        vecs.append(rgb)


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


if __name__ == "__main__":
    df = read_csv('vocabulary.csv')
    LABEL = df['Vertical1']

    data_folder = "data"
    tfrecord_files = [f for f in os.listdir(data_folder) if f.endswith('.tfrecord')]

    num = 0
    for tfrecord_file in tfrecord_files:
        tfrecord_path = os.path.join(data_folder, tfrecord_file)
        print(tfrecord_path)
        parsed_dataset = read_tfrecord(tfrecord_path)
        solve_tfrecord(parsed_dataset, LABEL)
        num += 1
        if num % 250 == 0:
            vecs_array = np.array(vecs, dtype=np.float32)
            output_fbin = f"res/YouTube-rgb/YouTube_{num // 250}.bin"
            write_fbin(output_fbin, vecs_array)
            msg = ['label', 'string']
            save_meta_to_txt(meta, f'res/YouTube-rgb/meta_{num // 250}.txt', msg)
            vecs.clear()
            meta.clear()

    vecs_array = np.array(vecs, dtype=np.float32)
    output_fbin = f"res/YouTube-rgb/YouTube_{4}.bin"
    write_fbin(output_fbin, vecs_array)
    msg = ['label', 'string']
    save_meta_to_txt(meta, f'res/YouTube-rgb/meta_{4}.txt', msg)
    vecs.clear()
    meta.clear()

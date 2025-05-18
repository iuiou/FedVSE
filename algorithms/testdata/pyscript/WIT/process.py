import csv
import requests
import numpy as np

vecs = []
meta = []
g_num = 4595
off = 0
ff = 64


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


def read_csv(file_path):
    """
    Reads a CSV file and returns a list of dictionaries, where each dictionary represents a row in the CSV file.
    """
    global g_num, ff, off
    with open(file_path, mode='r', newline='', encoding='utf-8') as file:
        reader = csv.reader(file)
        for row in reader:
            if row:
                if off < g_num + ff:
                    off += 1
                    continue
                vec = []
                parts = row[0].split('\t')
                link = parts[0]
                number = float(parts[1])
                vec.append(number)
                # 设置请求头
                headers = {
                    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.3'}

                # 获取图片的字节大小
                response = requests.get(link, headers=headers)
                if response.status_code == 200:
                    image_size = len(response.content)
                    meta.append(image_size)
                    print(f'{g_num}_图片大小: {image_size}')
                    g_num += 1
                else:
                    print(f"无法获取图片: {link}, 状态码: {response.status_code}")
                    ff += 1
                    print("ff:", ff)
                    continue
                for i in range(1, len(row)):
                    element = float(row[i])
                    vec.append(element)
                vecs.append(vec)
            if g_num % 100 == 0:
                vecs_array = np.array(vecs, dtype=np.float32)
                output_fbin = f"res/WIT_{g_num//100}.bin"
                write_fbin(output_fbin, vecs_array)
                print(f"Vectors saved to {output_fbin}")

                output_meta = f"res/meta_{g_num//100}.txt"
                save_meta_to_txt(meta, output_meta)
                print(f"Meta data num: {len(meta)}")
                print(f"Meta data saved to {output_meta}")

                vecs.clear()
                meta.clear()


def save_meta_to_txt(metadata, filename):
    """
    Saves the meta list to a .txt file with elements separated by spaces.

    :param meta: List of integers representing image sizes.
    :param filename: Path to the output .txt file.
    """
    with open(filename, 'w', encoding='utf-8') as f:
        f.write('\n'.join(map(str, metadata)))


if __name__ == '__main__':
    file_path_list = [
        'data/test_resnet_embeddings_part-00000.csv',
        'data/test_resnet_embeddings_part-00001.csv',
        'data/test_resnet_embeddings_part-00002.csv',
        'data/test_resnet_embeddings_part-00003.csv',
        'data/test_resnet_embeddings_part-00004.csv',
        'data/test_resnet_embeddings_part-00005.csv',
        'data/test_resnet_embeddings_part-00006.csv',
        'data/test_resnet_embeddings_part-00007.csv',
        'data/test_resnet_embeddings_part-00008.csv',
        'data/test_resnet_embeddings_part-00009.csv'
    ]
    for file_path in file_path_list:
        read_csv(file_path)
    print(ff)

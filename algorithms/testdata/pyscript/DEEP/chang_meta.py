import random


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
            f.write(m)
            f.write(' ')
            a = random.randint(0, 10000)
            f.write(str(a))
            f.write('\n')


if __name__ == '__main__':
    silos = 5
    for i in range(silos):
        meta_type, content = read_meta(f'res/meta_{i}.txt')
        msg = ['color', 'string', 'value', 'int']
        write_meta(content, f'res/meta_{i}.txt', msg)

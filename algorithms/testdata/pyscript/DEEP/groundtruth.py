import struct
import heapq

silos = 5
kkk = 128


class MaxHeap:
    def __init__(self):
        self.data = []

    def push(self, dist, vector_data):
        # 将dist取负后再插入堆，以模拟大顶堆
        heapq.heappush(self.data, (-dist, vector_data))

    def pop(self):
        if self.data:
            # 弹出时再将取负的dist转正
            dist, vector_data = heapq.heappop(self.data)
            return -dist, vector_data
        return None

    def top(self):
        if self.data:
            dist, vector_data = self.data[0]
            return -dist, vector_data
        return None

    def size(self):
        return len(self.data)

    def is_empty(self):
        return len(self.data) == 0


class VectorDataType:
    def __init__(self, vid, data):
        self.vid = vid
        self.data = data


def read_fivecs(file_name, data_list):
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


def dump_ground_truth_to_ivecs(file_name, answer_list):
    with open(file_name, 'wb') as file:
        i = 0
        for answer in answer_list:
            vec = [int(vid) for vid in answer]

            k = len(vec)
            file.write(struct.pack('i', k))
            file.write(struct.pack(f'{k}i', *vec))
            i += 1
            print(f"Writing ground truth: {i}")


def read_query(file_name):
    with open(file_name, 'r', encoding='utf-8') as file:
        content = file.read().splitlines()
        nvecs, dim = tuple(map(int, content[0].split(' ')))
        vecs = []
        conds = []
        for t in range(1, len(content)):
            line = content[t].split(' ')
            vec = list(map(float, line[:dim]))
            cond = line[dim:]
            vecs.append(vec)
            conds.append(cond)
    return vecs, conds


def read_meta(file_path):
    with open(file_path, 'r', encoding='utf-8') as file:
        content = file.read().splitlines()
        meta_type = content[1]
        content.pop(0)
        content.pop(0)
    return meta_type, content


def sta(cond, m):
    mm = m.split(" ")
    sci = str(cond[0])
    sci = sci.replace("=", "==")
    sci = sci.replace("color", '"' + str(mm[0]) + '"')
    cmp = str(cond[1])
    cmp = cmp.replace("value", str(mm[1]))
    return eval(sci) and eval(cmp)


# 计算两个向量的距离
def distance(vec1, vec2):
    return sum((a - b) ** 2 for a, b in zip(vec1, vec2))


if __name__ == '__main__':
    data_list = []
    meta_list = []
    ans = []
    qurey, conds = read_query("res/query.txt")
    heaps = [MaxHeap() for _ in range(len(qurey))]
    for j in range(silos):
        data_tmp = []
        read_fivecs(f"res/deep_{j}.fivecs", data_tmp)
        data_list.extend(data_tmp)
        meta_type, content = read_meta(f"res/meta_{j}.txt")
        meta_list.extend(content)
        for x in range(len(qurey)):
            print(f"Query {j}-{x}")
            heap = heaps[x]
            for y in range(len(data_list)):
                if sta(conds[x], meta_list[y]):
                    dist = distance(qurey[x], data_list[y].data)
                    if heap.size() < kkk:
                        heap.push(dist, data_list[y].vid)
                    else:
                        if dist < heap.top()[0]:
                            heap.pop()
                            heap.push(dist, data_list[y].vid)
    for x in range(len(qurey)):
        tmp = []
        heap = heaps[x]
        while not heap.is_empty():
            dist, vector_data = heap.pop()
            tmp.append(vector_data)
        tmp.reverse()
        ans.append(tmp)
    dump_ground_truth_to_ivecs(f"res/gt_{kkk}_{silos}.ivecs", ans)

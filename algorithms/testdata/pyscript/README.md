# README

###数据集

数据位置，数据位置已经移动到了`\home\dataset\hybrid`下，一共有四个数据集`WIT`，`deep`，`YouTube-aduio`和`YouTube-rgb`

### 代码

排版：四个文件夹分别负责处理四个数据集`LION`(已弃用)，`WIT`，`deep`，`YouTube`(包含`YouTube-aduio`和`YouTube-rgb`)

`process.py`: 处理原始数据，生成`.bin`文件——存放向量数据，生成`meta.txt`——存放结构化数据

`split.py`: 将原始向量数据分成多个`.fivecs`文件，与相应的多个`meta_i.txt`文件

`merge.py`: 将多个`.fivecs`文件与相应的`meta_i.txt`文件合并为一个大的`.fivecs`文件与相应的`meta_j.txt`

`generate_query.py`: 生成`query.txt`文件，包含100个query，示例可见`\home\dataset\hybrid\YouTube-audio\query.txt`

`groundtruth.py`: 生成对应的groundtruth文件


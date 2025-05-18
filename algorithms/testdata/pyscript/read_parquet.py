import pyarrow.parquet as pq
import pandas as pd

# 读取Parquet文件
table = pq.read_table('D:\python++\data_process\metadata_0.parquet')
df = table.to_pandas()

# 保存前5行数据为CSV文件，假设文件名是top5.csv
df.to_csv("top5.csv", index=False)
import torch
from transformers import CLIPProcessor, CLIPModel
import pyarrow.parquet as pq


def read_parquet(filename):
    table = pq.read_table(filename)
    df = table.to_pandas()
    return df


# 加载CLIP模型和处理器
model = CLIPModel.from_pretrained("openai/clip-vit-base-patch32")
processor = CLIPProcessor.from_pretrained("openai/clip-vit-base-patch32")

# 假设的句子列表
df = read_parquet('data/metadata_0.parquet')
caption_column = df['caption']
sentence_list = [sentence for sentence in caption_column]

for sentence in sentence_list:
    words = sentence.split()
    max_score = -float('inf')
    highest_word = ""
    for word in words:
        # 准备输入数据，将单词自身和所在句子作为文本对
        inputs = processor(text=[word], text_pair=[sentence], return_tensors="pt", padding=True)
        outputs = model(**inputs)
        # 这里简单使用余弦相似度计算得分，也可以尝试其他合适方法
        similarity_score = torch.cosine_similarity(outputs.logits_per_image, outputs.logits_per_text, dim=1)
        score = similarity_score.item()
        if score > max_score:
            max_score = score
            highest_word = word
    print(f"For sentence: {sentence}, the word with highest score is: {highest_word}")

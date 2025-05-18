import tensorflow as tf

# Define the feature description dictionary for decoding the TFRecord
feature_description = {
    "id": tf.io.FixedLenFeature([], tf.string),  # Video ID as a string
    "labels": tf.io.VarLenFeature(tf.int64),  # Labels as a sparse list of integers
    "mean_rgb": tf.io.FixedLenFeature([1024], tf.float32),  # 1024 float features
    "mean_audio": tf.io.FixedLenFeature([128], tf.float32),  # 128 float features
}


# Function to parse a single Example proto
def parse_example(example_proto):
    # Parse the input `tf.train.Example` proto using the feature description
    parsed_features = tf.io.parse_single_example(example_proto, feature_description)

    # Convert labels from sparse to dense (if needed)
    labels = tf.sparse.to_dense(parsed_features["labels"], default_value=0)

    return {
        "id": parsed_features["id"],
        "labels": labels,
        "mean_rgb": parsed_features["mean_rgb"],
        "mean_audio": parsed_features["mean_audio"]
    }


# Path to the TFRecord file
tfrecord_file = "YouTube/data/trainpj.tfrecord"

# Create a TFRecordDataset
dataset = tf.data.TFRecordDataset(tfrecord_file)

parsed_dataset = []
for example in dataset:
    parsed_example = parse_example(example)
    parsed_dataset.append(parsed_example)


# Iterate over the dataset and print the parsed examples
print(len(parsed_dataset))
for parsed_record in parsed_dataset:  # Display the first 5 records
    print(f"ID: {parsed_record['id'].numpy().decode('utf-8')}")
    print(f"Labels: {parsed_record['labels'].numpy()}")
    print(f"Mean RGB: {parsed_record['mean_rgb'].numpy()[:10]}...")  # Display first 10 for brevity
    print(f"Mean Audio: {parsed_record['mean_audio'].numpy()[:10]}...")  # Display first 10 for brevity
    print("-" * 50)
    break


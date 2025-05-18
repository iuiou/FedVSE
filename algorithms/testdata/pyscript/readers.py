import tensorflow as tf
import utils


def resize_axis(tensor, axis, new_size, fill_value=0):
    """Truncates or pads a tensor to new_size on on a given axis."""
    shape = tf.shape(tensor)

    pad_shape = shape[:]
    pad_shape = tf.tensor_scatter_nd_update(pad_shape, [[axis]], [tf.maximum(0, new_size - shape[axis])])

    shape = tf.tensor_scatter_nd_update(shape, [[axis]], [tf.minimum(shape[axis], new_size)])

    resized = tf.concat([
        tensor[:shape[axis]],
        tf.fill(tf.stack(pad_shape), tf.cast(fill_value, tensor.dtype))
    ], axis)

    # Update shape.
    new_shape = tensor.get_shape().as_list()
    new_shape[axis] = new_size
    resized.set_shape(new_shape)
    return resized


class BaseReader(object):
    """Inherit from this class when implementing new readers."""

    def prepare_reader(self, unused_filename_queue):
        """Create a thread for generating prediction and label tensors."""
        raise NotImplementedError()


class YT8MAggregatedFeatureReader(BaseReader):
    """Reads TFRecords of pre-aggregated Examples."""

    def __init__(self, num_classes=3862, feature_sizes=[1024, 128], feature_names=["mean_rgb", "mean_audio"]):
        assert len(feature_names) == len(feature_sizes), (
            "length of feature_names (={}) != length of feature_sizes (={})".format(
                len(feature_names), len(feature_sizes)))

        self.num_classes = num_classes
        self.feature_sizes = feature_sizes
        self.feature_names = feature_names

    def prepare_reader(self, filename_queue, batch_size=1024):
        """Creates a single reader thread for pre-aggregated YouTube 8M Examples."""
        dataset = tf.data.TFRecordDataset(filename_queue)
        dataset = dataset.batch(batch_size)
        dataset = dataset.map(self.prepare_serialized_examples)
        return dataset

    def prepare_serialized_examples(self, serialized_examples):
        """Parse a single video-level TF Example."""
        num_features = len(self.feature_names)
        assert num_features > 0, "self.feature_names is empty!"
        assert len(self.feature_names) == len(self.feature_sizes), (
            "length of feature_names (={}) != length of feature_sizes (={})".format(
                len(self.feature_names), len(self.feature_sizes)))

        feature_map = {
            "id": tf.io.FixedLenFeature([], tf.string),
            "labels": tf.io.VarLenFeature(tf.int64)
        }
        for feature_index in range(num_features):
            feature_map[self.feature_names[feature_index]] = tf.io.FixedLenFeature(
                [self.feature_sizes[feature_index]], tf.float32)

        features = tf.io.parse_example(serialized_examples, features=feature_map)
        labels = tf.sparse.to_dense(features["labels"])
        labels = tf.one_hot(indices=labels, depth=self.num_classes, dtype=tf.int32)
        labels = tf.reduce_sum(labels, axis=1)
        labels = tf.cast(labels, tf.float32)
        concatenated_features = tf.concat(
            [features[feature_name] for feature_name in self.feature_names], 1)

        output_dict = {
            "video_ids": features["id"],
            "video_matrix": concatenated_features,
            "labels": labels,
            "num_frames": tf.ones([tf.shape(serialized_examples)[0]])
        }

        return output_dict


class YT8MFrameFeatureReader(BaseReader):
    """Reads TFRecords of SequenceExamples."""

    def __init__(self, num_classes=3862, feature_sizes=[1024, 128], feature_names=["rgb", "audio"], max_frames=300,
                 segment_labels=False, segment_size=5):
        assert len(feature_names) == len(feature_sizes), (
            "length of feature_names (={}) != length of feature_sizes (={})".format(
                len(feature_names), len(feature_sizes)))

        self.num_classes = num_classes
        self.feature_sizes = feature_sizes
        self.feature_names = feature_names
        self.max_frames = max_frames
        self.segment_labels = segment_labels
        self.segment_size = segment_size

    def get_video_matrix(self, features, feature_size, max_frames, max_quantized_value, min_quantized_value):
        """Decodes features from an input string and quantizes it."""
        decoded_features = tf.reshape(
            tf.cast(tf.io.decode_raw(features, tf.uint8), tf.float32),
            [-1, feature_size])

        num_frames = tf.minimum(tf.shape(decoded_features)[0], max_frames)
        feature_matrix = utils.Dequantize(decoded_features, max_quantized_value, min_quantized_value)
        feature_matrix = resize_axis(feature_matrix, 0, max_frames)
        return feature_matrix, num_frames

    def prepare_reader(self, filename_queue, max_quantized_value=2, min_quantized_value=-2):
        """Creates a single reader thread for YouTube8M SequenceExamples."""
        dataset = tf.data.TFRecordDataset(filename_queue)
        dataset = dataset.map(lambda x: self.prepare_serialized_examples(x, max_quantized_value, min_quantized_value))
        return dataset

    def prepare_serialized_examples(self, serialized_example, max_quantized_value=2, min_quantized_value=-2):
        """Parse single serialized SequenceExample from the TFRecords."""
        context_features = {
            "id": tf.io.FixedLenFeature([], tf.string),
        }
        if self.segment_labels:
            context_features.update({
                "segment_labels": tf.io.VarLenFeature(tf.int64),
                "segment_start_times": tf.io.VarLenFeature(tf.int64),
                "segment_scores": tf.io.VarLenFeature(tf.float32)
            })
        else:
            context_features.update({"labels": tf.io.VarLenFeature(tf.int64)})
        sequence_features = {
            feature_name: tf.io.FixedLenSequenceFeature([], dtype=tf.string)
            for feature_name in self.feature_names
        }
        contexts, features = tf.io.parse_single_sequence_example(
            serialized_example,
            context_features=context_features,
            sequence_features=sequence_features)

        num_features = len(self.feature_names)
        assert num_features > 0, "No feature selected: feature_names is empty!"
        assert len(self.feature_names) == len(self.feature_sizes), (
            "length of feature_names (={}) != length of feature_sizes (={})".format(
                len(self.feature_names), len(self.feature_sizes)))

        num_frames = -1  # the number of frames in the video
        feature_matrices = [None] * num_features  # an array of different features
        for feature_index in range(num_features):
            feature_matrix, num_frames_in_this_feature = self.get_video_matrix(
                features[self.feature_names[feature_index]],
                self.feature_sizes[feature_index], self.max_frames,
                max_quantized_value, min_quantized_value)
            if num_frames == -1:
                num_frames = num_frames_in_this_feature

            feature_matrices[feature_index] = feature_matrix

        num_frames = tf.minimum(num_frames, self.max_frames)
        video_matrix = tf.concat(feature_matrices, 1)

        if self.segment_labels:
            start_times = contexts["segment_start_times"].values
            uniq_start_times, seg_idxs = tf.unique(start_times, out_idx=tf.dtypes.int64)
            segment_size = self.segment_size
            range_mtx = tf.expand_dims(uniq_start_times, axis=-1) + tf.expand_dims(
                tf.range(0, segment_size, dtype=tf.int64), axis=0)
            batch_video_matrix = tf.gather_nd(video_matrix, tf.expand_dims(range_mtx, axis=-1))
            num_segment = tf.shape(batch_video_matrix)[0]
            batch_video_ids = tf.reshape(tf.tile([contexts["id"]], [num_segment]), (num_segment,))
            batch_frames = tf.reshape(tf.tile([segment_size], [num_segment]), (num_segment,))

            label_indices = tf.stack([seg_idxs, contexts["segment_labels"].values], axis=-1)
            label_values = contexts["segment_scores"].values
            sparse_labels = tf.sparse.SparseTensor(label_indices, label_values, (num_segment, self.num_classes))
            batch_labels = tf.sparse.to_dense(sparse_labels, validate_indices=False)

            sparse_label_weights = tf.sparse.SparseTensor(
                label_indices, tf.ones_like(label_values, dtype=tf.float32), (num_segment, self.num_classes))
            batch_label_weights = tf.sparse.to_dense(sparse_label_weights, validate_indices=False)
        else:
            label_indices = contexts["labels"].values
            sparse_labels = tf.sparse.SparseTensor(
                tf.expand_dims(label_indices, axis=-1),
                tf.ones_like(contexts["labels"].values, dtype=tf.bool),
                (self.num_classes,))
            labels = tf.sparse.to_dense(sparse_labels, default_value=False, validate_indices=False)
            batch_video_ids = tf.expand_dims(contexts["id"], 0)
            batch_video_matrix = tf.expand_dims(video_matrix, 0)
            batch_labels = tf.expand_dims(labels, 0)
            batch_frames = tf.expand_dims(num_frames, 0)
            batch_label_weights = None

        output_dict = {
            "video_ids": batch_video_ids,
            "video_matrix": batch_video_matrix,
            "labels": batch_labels,
            "num_frames": batch_frames,
        }
        if batch_label_weights is not None:
            output_dict["label_weights"] = batch_label_weights

        return output_dict

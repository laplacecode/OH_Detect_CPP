from __future__ import annotations

import math

import cv2
import numpy as np


def select_tensors(outputs: list[np.ndarray]) -> tuple[np.ndarray, np.ndarray]:
    boxes = None
    proto = None
    for output in outputs:
        if output.ndim == 3 and boxes is None:
            boxes = output
        elif output.ndim == 4 and proto is None:
            proto = output

    if boxes is None or proto is None:
        raise RuntimeError("Expected one 3D boxes tensor and one 4D proto tensor.")
    return boxes, proto


def parse_detections(
    boxes_tensor: np.ndarray,
    proto_tensor: np.ndarray,
    confidence_threshold: float,
) -> list[dict]:
    if boxes_tensor.ndim != 3 or proto_tensor.ndim != 4:
        raise ValueError("Unexpected output tensor ranks.")

    _, proto_channels, _, _ = proto_tensor.shape

    if boxes_tensor.shape[1] < boxes_tensor.shape[2]:
        channels_first = True
        num_channels = boxes_tensor.shape[1]
        num_predictions = boxes_tensor.shape[2]
        boxes_view = boxes_tensor[0]
    else:
        channels_first = False
        num_predictions = boxes_tensor.shape[1]
        num_channels = boxes_tensor.shape[2]
        boxes_view = boxes_tensor[0]

    class_count = int(num_channels) - 4 - int(proto_channels)
    if class_count <= 0:
        raise ValueError("Failed to resolve class count from outputs.")

    detections: list[dict] = []
    for prediction_index in range(num_predictions):
        if channels_first:
            row = boxes_view[:, prediction_index]
        else:
            row = boxes_view[prediction_index, :]

        cx, cy, w, h = row[:4].tolist()
        class_scores = row[4 : 4 + class_count]
        class_id = int(np.argmax(class_scores))
        score = float(class_scores[class_id])
        if score < confidence_threshold:
            continue

        detections.append(
            {
                "class_id": class_id,
                "score": score,
                "box_xywh": [float(cx), float(cy), float(w), float(h)],
                "mask_coefficients": row[4 + class_count : 4 + class_count + proto_channels].astype(np.float32),
            }
        )

    return detections


def apply_nms(
    detections: list[dict],
    confidence_threshold: float,
    iou_threshold: float,
) -> list[dict]:
    if not detections:
        return []

    boxes: list[list[int]] = []
    scores: list[float] = []
    for detection in detections:
        cx, cy, w, h = detection["box_xywh"]
        x = int(max(0, math.floor(cx - (w * 0.5))))
        y = int(max(0, math.floor(cy - (h * 0.5))))
        bw = int(max(1, math.ceil(w)))
        bh = int(max(1, math.ceil(h)))
        boxes.append([x, y, bw, bh])
        scores.append(float(detection["score"]))

    indices = cv2.dnn.NMSBoxes(boxes, scores, confidence_threshold, iou_threshold)
    if len(indices) == 0:
        return []

    flat_indices = np.array(indices).reshape(-1).tolist()
    filtered: list[dict] = []
    for index in flat_indices:
        detection = dict(detections[int(index)])
        detection["box_xyxy"] = boxes[int(index)]
        filtered.append(detection)
    return filtered


def sigmoid(x: np.ndarray) -> np.ndarray:
    clipped = np.clip(x, -50.0, 50.0)
    return 1.0 / (1.0 + np.exp(-clipped))


def reconstruct_instance_mask(
    detection: dict,
    proto_tensor: np.ndarray,
    input_width: int,
    input_height: int,
    mask_threshold: float,
) -> np.ndarray:
    proto = proto_tensor[0]
    mask_channels, mask_height, mask_width = proto.shape
    proto_matrix = proto.reshape(mask_channels, mask_height * mask_width)
    coeff = detection["mask_coefficients"].reshape(1, mask_channels)
    mask = coeff @ proto_matrix
    mask = mask.reshape(mask_height, mask_width)
    mask = sigmoid(mask)
    mask = cv2.resize(mask, (input_width, input_height), interpolation=cv2.INTER_LINEAR)
    binary = (mask > mask_threshold).astype(np.uint8) * 255

    x, y, w, h = detection["box_xyxy"]
    x2 = min(input_width, x + w)
    y2 = min(input_height, y + h)
    cropped = np.zeros_like(binary, dtype=np.uint8)
    if x < x2 and y < y2:
        cropped[y:y2, x:x2] = binary[y:y2, x:x2]
    return cropped


def build_segmentation_maps(
    detections: list[dict],
    proto_tensor: np.ndarray,
    input_width: int,
    input_height: int,
    mask_threshold: float,
    class_values: dict[int, int],
) -> tuple[np.ndarray, np.ndarray]:
    seg_map = np.zeros((input_height, input_width), dtype=np.uint8)
    seg_map_255 = np.zeros((input_height, input_width), dtype=np.uint8)

    for detection in detections:
        instance_mask = reconstruct_instance_mask(
            detection,
            proto_tensor,
            input_width,
            input_height,
            mask_threshold,
        )
        class_value = class_values.get(detection["class_id"], detection["class_id"] + 1)
        mask_region = instance_mask > 0
        seg_map[mask_region] = np.uint8(class_value)
        seg_map_255[mask_region] = np.uint8(255)

    return seg_map, seg_map_255

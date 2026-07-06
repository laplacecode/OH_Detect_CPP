from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

import cv2
import numpy as np
import onnxruntime as ort

from pdf_protocol_codec import (
    InitParas,
    ReceiveDataFields,
    SendDataFields,
    decode_init_paras,
    decode_receive_data,
    encode_send_data,
)
from seg_runtime_utils import (
    apply_nms,
    build_segmentation_maps,
    parse_detections,
    select_tensors,
)


REPO_ROOT = Path(__file__).resolve().parents[1]


@dataclass
class RunArtifacts:
    drawing_image_8bit: np.ndarray
    raw_image_8bit: np.ndarray
    enhanced_image_8bit: np.ndarray
    seg_image: np.ndarray
    seg_image_to255: np.ndarray
    send_data: SendDataFields


@dataclass
class ClassGeometry:
    component_count: int
    centroid_x_offset: float
    centroid_y_offset: float


class PdfProtocolAdapter:
    """按 PDF 接口语义组织的内部验证适配器。

    这个类不宣称与原始生产算法等价，只用于验证：
    1. 固定顺序的 UTF-8 QByteArray 协议；
    2. 16 位图输入 -> ONNX 推理 -> 5 张输出图；
    3. sendData 固定顺序结果字段。
    """

    def __init__(self) -> None:
        self.init_paras_value: InitParas | None = None
        self.receive_fields: ReceiveDataFields | None = None
        self.model_path: Path | None = None
        self.session: ort.InferenceSession | None = None

        self.raw16_image: np.ndarray | None = None
        self.standardized16_image: np.ndarray | None = None
        self.raw8_image: np.ndarray | None = None
        self.enhanced8_image: np.ndarray | None = None
        self.crop_rect_xywh: tuple[int, int, int, int] | None = None
        self.run_artifacts: RunArtifacts | None = None

    def init_paras(self, raw: bytes) -> bool:
        self.init_paras_value = decode_init_paras(raw)
        model_path = Path(self.init_paras_value.onnx_model_path)
        if not model_path.is_absolute():
            model_path = (REPO_ROOT / model_path).resolve()
        if not model_path.exists():
            return False

        self.model_path = model_path
        self.session = ort.InferenceSession(str(model_path), providers=["CPUExecutionProvider"])
        return True

    def recive_data(self, images: list[np.ndarray], raw: bytes) -> bool:
        if not images:
            return False
        image = images[0]
        if image.dtype != np.uint16 or image.ndim != 2:
            return False

        self.receive_fields = decode_receive_data(raw)
        self.raw16_image = image.copy()
        self.standardized16_image = self._standardize_size(image)
        self.raw8_image = self._window_to_8bit(self.standardized16_image)
        self.enhanced8_image = self._enhance_image(self.raw8_image)
        return True

    def run(self) -> bool:
        if self.session is None or self.enhanced8_image is None or self.init_paras_value is None or self.receive_fields is None:
            return False

        crop_image, crop_rect = self._build_768_input(self.enhanced8_image)
        self.crop_rect_xywh = crop_rect

        input_tensor = crop_image.astype(np.float32) / 255.0
        input_tensor = np.transpose(input_tensor, (2, 0, 1))[None, ...]

        input_name = self.session.get_inputs()[0].name
        outputs = self.session.run(None, {input_name: input_tensor})
        boxes_tensor, proto_tensor = select_tensors(outputs)
        detections = parse_detections(boxes_tensor, proto_tensor, 0.001)
        detections = apply_nms(detections, 0.001, 0.45)

        seg_crop, seg_crop_255 = build_segmentation_maps(
            detections,
            proto_tensor,
            768,
            768,
            0.5,
            {},
        )

        seg_full = self._restore_full_size(seg_crop)
        seg_full_255 = self._restore_full_size(seg_crop_255)
        drawing = self._build_drawing_image(self.enhanced8_image, seg_full_255)
        send_fields = self._build_send_fields(seg_full, seg_full_255, detections)

        self.run_artifacts = RunArtifacts(
            drawing_image_8bit=drawing,
            raw_image_8bit=self.raw8_image.copy(),
            enhanced_image_8bit=self.enhanced8_image.copy(),
            seg_image=seg_full,
            seg_image_to255=seg_full_255,
            send_data=send_fields,
        )
        return True

    def send_data(self) -> tuple[list[np.ndarray], bytes]:
        if self.run_artifacts is None:
            raise RuntimeError("run() must complete before send_data().")

        images = [
            self.run_artifacts.drawing_image_8bit,
            self.run_artifacts.raw_image_8bit,
            self.run_artifacts.enhanced_image_8bit,
            self.run_artifacts.seg_image,
            self.run_artifacts.seg_image_to255,
        ]
        raw = encode_send_data(self.run_artifacts.send_data)
        return images, raw

    def _standardize_size(self, image: np.ndarray) -> np.ndarray:
        height, width = image.shape[:2]
        target_size = (1536, 1152)
        if (width, height) in {(1536, 1152), (2048, 1152)}:
            return image.copy()
        return cv2.resize(image, target_size, interpolation=cv2.INTER_LINEAR)

    def _window_to_8bit(self, image16: np.ndarray) -> np.ndarray:
        assert self.init_paras_value is not None
        width = max(float(self.init_paras_value.window_width), 1.0)
        level = float(self.init_paras_value.window_level)
        lower = level - width / 2.0
        upper = level + width / 2.0
        clipped = np.clip(image16.astype(np.float32), lower, upper)
        scaled = (clipped - lower) / max(upper - lower, 1e-6)
        return np.clip(scaled * 255.0, 0.0, 255.0).astype(np.uint8)

    def _enhance_image(self, raw8: np.ndarray) -> np.ndarray:
        clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
        enhanced = clahe.apply(raw8)
        return cv2.GaussianBlur(enhanced, (3, 3), 0)

    def _build_768_input(self, image8: np.ndarray) -> tuple[np.ndarray, tuple[int, int, int, int]]:
        height, width = image8.shape[:2]
        crop_size = min(768, width, height)
        start_x = max((width - crop_size) // 2, 0)
        start_y = max((height - crop_size) // 2, 0)
        crop = image8[start_y : start_y + crop_size, start_x : start_x + crop_size]
        if crop_size != 768:
            crop = cv2.resize(crop, (768, 768), interpolation=cv2.INTER_LINEAR)
        crop_bgr = cv2.cvtColor(crop, cv2.COLOR_GRAY2BGR)
        return crop_bgr, (start_x, start_y, crop_size, crop_size)

    def _restore_full_size(self, seg_crop: np.ndarray) -> np.ndarray:
        if self.enhanced8_image is None or self.crop_rect_xywh is None:
            raise RuntimeError("Missing crop metadata.")
        start_x, start_y, crop_w, crop_h = self.crop_rect_xywh
        full = np.zeros_like(self.enhanced8_image, dtype=np.uint8)
        restored_crop = seg_crop
        if (crop_w, crop_h) != (768, 768):
            restored_crop = cv2.resize(seg_crop, (crop_w, crop_h), interpolation=cv2.INTER_NEAREST)
        full[start_y : start_y + crop_h, start_x : start_x + crop_w] = restored_crop
        return full

    def _build_drawing_image(self, enhanced8: np.ndarray, seg255: np.ndarray) -> np.ndarray:
        drawing = cv2.cvtColor(enhanced8, cv2.COLOR_GRAY2BGR)
        contours, _ = cv2.findContours(seg255, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        cv2.drawContours(drawing, contours, -1, (0, 255, 0), 2)
        return drawing

    def _build_send_fields(self, seg_map: np.ndarray, seg255: np.ndarray, detections: list[dict]) -> SendDataFields:
        assert self.init_paras_value is not None
        assert self.receive_fields is not None

        non_zero = int(np.count_nonzero(seg255))
        has_mask = non_zero > 0
        scan_result = 1 if self.receive_fields.product_qr else 2
        positive_geometry = self._compute_class_geometry(seg_map, class_value=1)
        negative_geometry = self._compute_class_geometry(seg_map, class_value=2)

        positive_horizontal = (
            positive_geometry.centroid_x_offset + self.init_paras_value.horizontal_positive_alignment_compensation
        )
        negative_horizontal = (
            negative_geometry.centroid_x_offset + self.init_paras_value.horizontal_negative_alignment_compensation
        )
        positive_vertical = (
            positive_geometry.centroid_y_offset + self.init_paras_value.vertical_positive_alignment_compensation
        )
        negative_vertical = (
            negative_geometry.centroid_y_offset + self.init_paras_value.vertical_negative_alignment_compensation
        )

        oh_sub_result = self._evaluate_oh_proxy(seg255)
        alignment_sub_result = self._evaluate_alignment_proxy(
            positive_geometry,
            negative_geometry,
            positive_horizontal,
            negative_horizontal,
            positive_vertical,
            negative_vertical,
        )
        layer_count_sub_result = self._evaluate_layer_counts(positive_geometry.component_count, negative_geometry.component_count)
        final_result = 1 if all(v == 1 for v in [scan_result, oh_sub_result, alignment_sub_result, layer_count_sub_result]) else 2

        positive_layers = positive_geometry.component_count
        negative_layers = negative_geometry.component_count

        return SendDataFields(
            camera_name=self.receive_fields.camera_name,
            current_time=datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            product_qr=self.receive_fields.product_qr,
            scan_result=scan_result,
            image_gray_value=float(np.mean(self.standardized16_image)) if self.standardized16_image is not None else 0.0,
            tube_voltage=self.receive_fields.tube_voltage,
            tube_current=self.receive_fields.tube_current,
            tube_life=self.receive_fields.tube_life,
            horizontal_oh_max_limit=self.init_paras_value.horizontal_oh_max_limit,
            horizontal_oh_min_limit=self.init_paras_value.horizontal_oh_min_limit,
            vertical_oh_max_limit=self.init_paras_value.vertical_oh_max_limit,
            vertical_oh_min_limit=self.init_paras_value.vertical_oh_min_limit,
            oh_sub_result=oh_sub_result,
            horizontal_positive_alignment_value=positive_horizontal,
            horizontal_negative_alignment_value=negative_horizontal,
            vertical_positive_alignment_value=positive_vertical,
            vertical_negative_alignment_value=negative_vertical,
            alignment_sub_result=alignment_sub_result,
            positive_layer_count=positive_layers,
            negative_layer_count=negative_layers,
            layer_count_sub_result=layer_count_sub_result,
            fixture_id=self.receive_fields.fixture_id,
            final_result_flag=final_result,
        )

    def _compute_class_geometry(self, seg_map: np.ndarray, class_value: int) -> ClassGeometry:
        mask = (seg_map == class_value).astype(np.uint8)
        if np.count_nonzero(mask) == 0:
            return ClassGeometry(component_count=0, centroid_x_offset=0.0, centroid_y_offset=0.0)

        component_count, _ = cv2.connectedComponents(mask)
        ys, xs = np.where(mask > 0)
        center_x = (seg_map.shape[1] - 1) * 0.5
        center_y = (seg_map.shape[0] - 1) * 0.5
        scale = float(self.init_paras_value.pixel_to_physical_scale)
        return ClassGeometry(
            component_count=max(component_count - 1, 0),
            centroid_x_offset=(float(np.mean(xs)) - center_x) * scale,
            centroid_y_offset=(float(np.mean(ys)) - center_y) * scale,
        )

    def _evaluate_alignment_proxy(
        self,
        positive_geometry: ClassGeometry,
        negative_geometry: ClassGeometry,
        positive_horizontal: float,
        negative_horizontal: float,
        positive_vertical: float,
        negative_vertical: float,
    ) -> int:
        assert self.init_paras_value is not None

        checks: list[bool] = []
        if self.init_paras_value.enable_horizontal_alignment:
            checks.append(
                positive_geometry.component_count > 0
                and abs(positive_horizontal) <= self.init_paras_value.horizontal_positive_alignment_limit
            )
            if self.init_paras_value.horizontal_negative_alignment_limit > 0:
                checks.append(
                    negative_geometry.component_count > 0
                    and abs(negative_horizontal) <= self.init_paras_value.horizontal_negative_alignment_limit
                )
        if self.init_paras_value.enable_vertical_alignment:
            checks.append(
                positive_geometry.component_count > 0
                and abs(positive_vertical) <= self.init_paras_value.vertical_positive_alignment_limit
            )
            if self.init_paras_value.vertical_negative_alignment_limit > 0:
                checks.append(
                    negative_geometry.component_count > 0
                    and abs(negative_vertical) <= self.init_paras_value.vertical_negative_alignment_limit
                )
        if not checks:
            return 1
        return 1 if all(checks) else 2

    def _evaluate_layer_counts(self, positive_count: int, negative_count: int) -> int:
        assert self.init_paras_value is not None
        if not self.init_paras_value.enable_layer_count_check:
            return 1

        checks = [
            positive_count >= int(round(self.init_paras_value.positive_layer_target)),
            negative_count >= int(round(self.init_paras_value.negative_layer_target)),
        ]
        return 1 if all(checks) else 2

    def _evaluate_oh_proxy(self, seg255: np.ndarray) -> int:
        assert self.init_paras_value is not None
        if not self.init_paras_value.enable_horizontal_oh and not self.init_paras_value.enable_vertical_oh:
            return 1
        if np.count_nonzero(seg255) == 0:
            return 2

        ys, xs = np.where(seg255 > 0)
        scale = float(self.init_paras_value.pixel_to_physical_scale)
        left_margin = float(np.min(xs)) * scale + self.init_paras_value.horizontal_oh_compensation
        top_margin = float(np.min(ys)) * scale + self.init_paras_value.vertical_oh_compensation

        checks: list[bool] = []
        if self.init_paras_value.enable_horizontal_oh:
            checks.append(self.init_paras_value.horizontal_oh_min_limit <= left_margin <= self.init_paras_value.horizontal_oh_max_limit)
        if self.init_paras_value.enable_vertical_oh:
            checks.append(self.init_paras_value.vertical_oh_min_limit <= top_margin <= self.init_paras_value.vertical_oh_max_limit)
        return 1 if all(checks) else 2

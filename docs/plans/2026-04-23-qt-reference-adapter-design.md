# Qt Reference Adapter Design

> Note: As of 2026-04-25, repository verification directories were split to root-level `python_verify/` and `cpp_verify/`. This design doc predates that rename, but the delivered adapter scope remains accurate.

**Goal:** Add a customer-integratable Qt reference adapter under `delivery/` that preserves the existing `Qt + QByteArray` interface shape and reproduces the repository's verified protocol flow around the delivered ONNX model.

**Scope Boundary:**
- `delivery/package/` remains the authoritative formal delivery package.
- The new adapter lives outside `delivery/package/` to avoid mixing formal artifacts with reference source code.
- The adapter is a reference implementation for integration and protocol reproduction. It is not a claim of equivalence to the original production OH algorithm.

## Directory Layout

Create a new sibling directory:

```text
delivery/
  package/
  reference_qt_adapter/
```

Planned contents:

```text
delivery/reference_qt_adapter/
  README.md
  OH_Detect_Station_Module.cpp
  protocol_codec.h
  protocol_codec.cpp
  image_pipeline.h
  image_pipeline.cpp
  seg_runtime.h
  seg_runtime.cpp
```

## Interface Strategy

- Keep `delivery/package/base.h` and `delivery/package/OH_Detect_Station_Module.h` as the authoritative interface definitions.
- The new `.cpp` implementation will include those headers and implement:
  - `initParas(QByteArray& config)`
  - `reciveData(std::vector<cv::Mat>& images, QByteArray& data)`
  - `run()`
  - `sendData(std::vector<cv::Mat>& images, QByteArray& data)`
- Keep the existing exported factory functions:
  - `create()`
  - `destory()`

## Functional Mapping

The C++ reference adapter mirrors the current verified Python flow:

1. Decode fixed-order `QByteArray` parameters from `initParas`.
2. Load the ONNX model with ONNX Runtime.
3. Accept a single-channel `CV_16U` image in `reciveData`.
4. Standardize image size, apply windowing, and generate the enhanced 8-bit image.
5. Build the `768 x 768` model input.
6. Run ONNX inference using the standard YOLOv8-seg dual outputs.
7. Parse detection tensor and proto tensor, apply NMS, and reconstruct segmentation maps.
8. Restore segmentation results to the full image canvas.
9. Build the five protocol output images.
10. Encode fixed-order `sendData` bytes.

## Component Responsibilities

### `protocol_codec.*`

- Decode `initParas` fixed-order fields from `QByteArray`.
- Decode `reciveData` fixed-order fields from `QByteArray`.
- Encode `sendData` fixed-order result fields into `QByteArray`.
- Use UTF-8 for string conversion.

### `image_pipeline.*`

- Validate input image shape and type.
- Standardize image size to the verified pipeline expectations.
- Convert 16-bit grayscale to 8-bit using window width and level.
- Enhance the 8-bit image.
- Build the centered `768 x 768` crop used for model inference.
- Restore cropped segmentation output back to the full image size.
- Build `m_drawingImage_8bit`, `m_rawImage_8bit`, `m_enhancedImage_8bit`, `m_segImage`, `m_segImageTo255`.

### `seg_runtime.*`

- Create and own the ONNX Runtime session.
- Read model input/output metadata.
- Run inference on the prepared tensor.
- Interpret the standard YOLOv8-seg dual outputs:
  - boxes tensor
  - proto tensor
- Apply NMS.
- Reconstruct instance masks and combine them into segmentation maps.

### `OH_Detect_Station_Module.cpp`

- Own adapter state across the interface lifecycle.
- Glue together protocol decode, image pipeline, ONNX runtime, and result encode steps.
- Preserve expected call ordering and return `false` on invalid state transitions.

## Error Handling

- `initParas` returns `false` when:
  - the byte payload is malformed
  - the model path is missing
  - ONNX Runtime session creation fails
- `reciveData` returns `false` when:
  - no image is provided
  - the image is not single-channel `CV_16U`
  - the payload cannot be decoded
- `run` returns `false` when:
  - initialization or input state is incomplete
  - ONNX inference fails
  - output tensors do not match the expected ranks
- `sendData` returns `false` when:
  - `run` has not completed successfully

## Delivery Statement

The adapter `README.md` must state:

- it is a customer-integratable reference source implementation
- it preserves the Qt interface shape defined by the delivered headers
- it relies on the delivered `model/best.onnx`
- it reproduces the repository's verified protocol flow
- it is not a claim of equivalence to the original production OH algorithm

## Verification Targets

The implementation is considered aligned when it can reproduce the current repository validation expectations:

- the delivered ONNX model loads successfully
- the model input is `[1, 3, 768, 768]`
- the model exposes standard YOLOv8-seg dual outputs
- the adapter can produce the five expected output images
- the adapter can emit a fixed-order `sendData` byte payload

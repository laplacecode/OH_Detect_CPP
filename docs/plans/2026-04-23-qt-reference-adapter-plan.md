# Qt Reference Adapter Implementation Plan

> Note: As of 2026-04-25, repository verification directories were split to root-level `python_verify/` and `cpp_verify/`. This historical plan still describes the adapter implementation tasks correctly; verification path examples should be interpreted against the current directory names.

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a customer-integratable Qt reference adapter under `delivery/reference_qt_adapter/` that preserves the delivered `Qt + QByteArray` interface shape and reproduces the repository's verified protocol flow around `delivery/package/model/best.onnx`.

**Architecture:** Keep `delivery/package/` as the authoritative formal delivery package and add a sibling `delivery/reference_qt_adapter/` directory containing only reference source code. Implement the interface declared in `delivery/package/OH_Detect_Station_Module.h` by translating the current Python validation flow into modular C++ components for protocol codec, image preprocessing, ONNX runtime inference, segmentation reconstruction, and result packaging.

**Tech Stack:** Qt (`QByteArray`, `QString`, `QVector`), OpenCV, ONNX Runtime, delivered headers in `delivery/package/`.

---

### Task 1: Create the reference adapter documentation skeleton

**Files:**
- Create: `delivery/reference_qt_adapter/README.md`

**Step 1: Write the content that should initially be missing**

Document these sections:
- purpose and scope boundary
- required dependencies
- source file overview
- integration lifecycle: `initParas -> reciveData -> run -> sendData`
- explicit statement that this is a reference adapter, not the original production algorithm

**Step 2: Review the existing delivery wording to confirm the current repository boundary**

Read:
- `delivery/package/README.md`
- `README_交付说明.md`

Expected: the README language matches existing delivery boundaries and does not claim production equivalence.

**Step 3: Add the minimal README**

Write the README with only the sections above and no build-system promises.

**Step 4: Re-read the README in UTF-8**

Run: `Get-Content delivery\\reference_qt_adapter\\README.md -Encoding UTF8`

Expected: Chinese text displays correctly and the boundary statement is explicit.

**Step 5: Do not commit**

This repository forbids automated git commands in the workspace.

### Task 2: Add protocol codec declarations

**Files:**
- Create: `delivery/reference_qt_adapter/protocol_codec.h`

**Step 1: Write the failing design check**

List the exact data groups that the codec layer must cover:
- init parameters
- receive fields
- send fields

Expected: there is currently no C++ codec declaration file.

**Step 2: Define the minimal data structures**

Add plain structs for:
- init parameters
- receive fields
- send fields

Use names that stay close to the Python adapter vocabulary.

**Step 3: Declare codec functions**

Declare functions for:
- decoding `QByteArray` into init parameters
- decoding `QByteArray` into receive fields
- encoding send fields into `QByteArray`

**Step 4: Re-read the header**

Run: `Get-Content delivery\\reference_qt_adapter\\protocol_codec.h -Encoding UTF8`

Expected: the declarations are minimal, readable, and use Qt types at the boundary only.

**Step 5: Do not commit**

This repository forbids automated git commands in the workspace.

### Task 3: Implement protocol codec logic

**Files:**
- Modify: `delivery/reference_qt_adapter/protocol_codec.h`
- Create: `delivery/reference_qt_adapter/protocol_codec.cpp`

**Step 1: Write the failing verification target**

Use the existing decoded JSON outputs as the target behavior for field order and UTF-8 handling:
- `python_verify/pdf_protocol_preview/initParas_decoded.json`
- `python_verify/pdf_protocol_preview/reciveData_decoded.json`
- `python_verify/pdf_protocol_preview/sendData_decoded.json`

Expected: no C++ codec implementation exists yet.

**Step 2: Implement minimal UTF-8 line-based decode helpers**

Implement:
- splitting raw `QByteArray` into ordered UTF-8 fields
- scalar conversion helpers for `bool`, `int`, and `double`

**Step 3: Implement the three codec entry points**

Implement:
- init decode
- receive decode
- send encode

**Step 4: Re-read the implementation**

Run: `Get-Content delivery\\reference_qt_adapter\\protocol_codec.cpp -Encoding UTF8`

Expected: field order is explicit and no hidden parsing rules are introduced.

**Step 5: Do not commit**

This repository forbids automated git commands in the workspace.

### Task 4: Add image pipeline declarations

**Files:**
- Create: `delivery/reference_qt_adapter/image_pipeline.h`

**Step 1: Write the failing design check**

List the pipeline stages that must exist:
- input validation
- size normalization
- windowing
- enhancement
- `768 x 768` crop build
- crop restore
- output image generation

Expected: there is currently no C++ image pipeline declaration file.

**Step 2: Declare the minimal structs and functions**

Declare only what is needed to carry:
- standardized images
- crop metadata
- restored segmentation outputs

**Step 3: Re-read the header**

Run: `Get-Content delivery\\reference_qt_adapter\\image_pipeline.h -Encoding UTF8`

Expected: responsibilities are clear and no protocol concerns leak into this file.

**Step 4: Do not commit**

This repository forbids automated git commands in the workspace.

### Task 5: Implement image pipeline logic

**Files:**
- Modify: `delivery/reference_qt_adapter/image_pipeline.h`
- Create: `delivery/reference_qt_adapter/image_pipeline.cpp`

**Step 1: Write the failing verification target**

Use the current Python adapter behavior as the expected flow:
- standardize image size
- apply window width/level
- generate enhanced 8-bit image
- create a centered `768 x 768` BGR crop
- restore segmentation crop to full-size canvas

Expected: no C++ implementation exists yet.

**Step 2: Implement minimal preprocessing helpers**

Implement:
- size normalization
- 16-bit to 8-bit windowing
- enhancement

**Step 3: Implement crop build and restore helpers**

Implement:
- centered crop metadata calculation
- crop extraction
- full-size restoration

**Step 4: Implement output image builders**

Implement:
- drawing image creation
- segmentation visualization outputs

**Step 5: Re-read the implementation**

Run: `Get-Content delivery\\reference_qt_adapter\\image_pipeline.cpp -Encoding UTF8`

Expected: logic stays aligned with the verified Python flow and avoids unrelated production heuristics.

**Step 6: Do not commit**

This repository forbids automated git commands in the workspace.

### Task 6: Add segmentation runtime declarations

**Files:**
- Create: `delivery/reference_qt_adapter/seg_runtime.h`

**Step 1: Write the failing design check**

List the required runtime concerns:
- session creation
- input/output metadata
- output tensor selection
- detection parsing
- NMS
- mask reconstruction
- segmentation map build

Expected: there is currently no C++ segmentation runtime declaration file.

**Step 2: Declare the minimal runtime state and functions**

Keep the interface small and aligned with the current ONNX model expectations.

**Step 3: Re-read the header**

Run: `Get-Content delivery\\reference_qt_adapter\\seg_runtime.h -Encoding UTF8`

Expected: the header reflects standard YOLOv8-seg dual-output handling and nothing broader.

**Step 4: Do not commit**

This repository forbids automated git commands in the workspace.

### Task 7: Implement segmentation runtime logic

**Files:**
- Modify: `delivery/reference_qt_adapter/seg_runtime.h`
- Create: `delivery/reference_qt_adapter/seg_runtime.cpp`

**Step 1: Write the failing verification target**

Use the current verified ONNX expectations:
- one input: `[1, 3, 768, 768]`
- two outputs: standard YOLOv8-seg dual outputs

Expected: no C++ runtime implementation exists yet.

**Step 2: Implement session initialization**

Implement:
- model loading
- CPU provider setup
- input/output metadata extraction

**Step 3: Implement output parsing**

Implement:
- 3D boxes tensor selection
- 4D proto tensor selection
- class score parsing
- mask coefficient extraction

**Step 4: Implement NMS and mask reconstruction**

Implement:
- bounding-box conversion
- OpenCV NMS
- proto reconstruction
- binary mask crop
- segmentation map accumulation

**Step 5: Re-read the implementation**

Run: `Get-Content delivery\\reference_qt_adapter\\seg_runtime.cpp -Encoding UTF8`

Expected: output handling is explicit and tied to the current model contract.

**Step 6: Do not commit**

This repository forbids automated git commands in the workspace.

### Task 8: Implement the Qt adapter class

**Files:**
- Create: `delivery/reference_qt_adapter/OH_Detect_Station_Module.cpp`
- Reference: `delivery/package/OH_Detect_Station_Module.h`
- Reference: `delivery/package/base.h`

**Step 1: Write the failing state-transition checklist**

The implementation must reject these invalid transitions:
- `run` before `initParas`
- `run` before `reciveData`
- `sendData` before successful `run`

Expected: there is currently no C++ implementation file.

**Step 2: Implement adapter-owned state**

Track:
- decoded init parameters
- decoded receive fields
- model session
- raw and processed images
- segmentation outputs
- encoded send-data source values

**Step 3: Implement `initParas` and `reciveData`**

Wire:
- protocol decode
- model path resolution
- ONNX session initialization
- input image validation
- preprocessing

**Step 4: Implement `run`**

Wire:
- crop build
- inference
- segmentation restore
- proxy measurement computation
- send-field preparation

**Step 5: Implement `sendData` and exported factory functions**

Wire:
- five output images
- encoded `QByteArray`
- `create`
- `destory`

**Step 6: Re-read the implementation**

Run: `Get-Content delivery\\reference_qt_adapter\\OH_Detect_Station_Module.cpp -Encoding UTF8`

Expected: the file includes the delivered headers and preserves the Qt interface shape.

**Step 7: Do not commit**

This repository forbids automated git commands in the workspace.

### Task 9: Verify repository-facing alignment

**Files:**
- Read: `delivery/reference_qt_adapter/README.md`
- Read: `delivery/reference_qt_adapter/*.h`
- Read: `delivery/reference_qt_adapter/*.cpp`

**Step 1: Verify UTF-8 readability**

Run UTF-8 reads against all new files.

Expected: Chinese text and comments display correctly.

**Step 2: Verify scope statements**

Confirm the README states:
- reference adapter
- customer integratable
- relies on delivered ONNX
- not equivalent to the original production algorithm

**Step 3: Verify no formal delivery files were rewritten**

Read:
- `delivery/package/base.h`
- `delivery/package/OH_Detect_Station_Module.h`
- `delivery/package/model/best.onnx`

Expected: no formal delivery artifacts are modified by the adapter implementation work.

**Step 4: Do not commit**

This repository forbids automated git commands in the workspace.

# Repository Guidelines

## Project Structure & Module Organization

This repository is organized around a formal delivery package plus separate Python and C++ verification folders.

- `delivery/package/`: customer-facing delivery artifacts only. This includes `base.h`, `OH_Detect_Station_Module.h`, `doc/`, and `model/best.onnx`.
- `delivery/reference_qt_adapter/`: customer-side Qt reference adapter source.
- `python_verify/`: internal Python utilities for training, ONNX export, IO inspection, and protocol preview generation.
- `cpp_verify/`: internal MinGW/Qt/OpenCV/ONNX Runtime verification entry for the C++ adapter flow.
- `runs/`: generated training and export outputs such as `runs/segment/test_Screw_seg/weights/best.pt`.
- Root files `base.h`, `OH_Detect_Station_Module.h`, and `README_交付说明.md` are the authoritative references for the original interface and delivery boundaries.

Do not mix verification code into `delivery/package/`.

## Build, Test, and Development Commands

- `python python_verify/train_yolov8_seg.py --data <data.yaml> --device cpu --epochs 200 --batch 4`
  Trains a YOLOv8 segmentation model.
- `python python_verify/export_yolov8_seg_onnx.py --weights runs/segment/test_Screw_seg/weights/best.pt`
  Exports the trained model to ONNX.
- `python python_verify/check_onnx_io.py --model runs/segment/test_Screw_seg/weights/best.onnx`
  Prints ONNX input/output names, types, and shapes.
- `python python_verify/run_pdf_protocol_verify.py --promote-8bit`
  Runs the preview pipeline against `python_verify/adapter_input/`.
- `cpp_verify\\build_mingw_manual_qt6.cmd`
  Builds the C++ verification entry with MinGW.

## Coding Style & Naming Conventions

- Use UTF-8 for all new documentation and scripts.
- Follow existing naming: snake_case for Python files, PascalCase-style identifiers only when matching upstream C++ API names.
- Keep comments short and factual. Prefer Chinese for repo-specific documentation and inline explanations.
- Do not rewrite the original delivery headers to simplify the interface; keep PDF-compatible protocol definitions intact.

## Testing Guidelines

- There is no dedicated unit test suite yet; validation is script-based.
- Before claiming changes are usable, run at least the ONNX IO check and one preview/build command relevant to the changed area.
- Treat `delivery/package/` as immutable delivery content; validate changes in `python_verify/` or `cpp_verify/` first.

## Commit & Pull Request Guidelines

- This repository currently has no commit history, so no established convention exists yet.
- Use short imperative commit messages, e.g. `docs: update delivery package guide`.
- PRs should state whether the change affects `delivery/package/`, `python_verify/`, or `cpp_verify/`, and list any regenerated artifacts.

## Security & Configuration Tips

- Do not commit customer data, private datasets, or temporary exports.
- Keep formal delivery limited to `delivery/package/`; keep experiments, previews, and training outputs under `python_verify/`, `cpp_verify/`, or `runs/`.

## 仓库硬约束
- 强制使用 UTF-8 编码。
- 在 Windows PowerShell 中读取中文文件、日志或 Markdown 前，必须先切换 UTF-8 输出环境。
- 禁止在这个工作区里通过自动化工具执行任何 `git` 命令，包括只读命令。
- 禁止使用 `apply_patch` 删除文件；删除文件时统一使用显式路径的 `Remove-Item -LiteralPath ...`。

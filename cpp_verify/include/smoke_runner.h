#ifndef CPP_VERIFY_SMOKE_RUNNER_H
#define CPP_VERIFY_SMOKE_RUNNER_H

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef CPP_VERIFY_FULL_BUILD
#include "../../delivery/package/OH_Detect_Station_Module.h"
#include <QByteArray>
#include <QString>
#include <opencv2/opencv.hpp>
#endif

namespace cpp_verify {

struct SmokeOptions {
    bool show_help = false;
    std::filesystem::path model_path;
    std::filesystem::path input_dir;
    std::filesystem::path output_dir;
};

inline std::filesystem::path repo_root_from_exe(const std::filesystem::path& exe_path) {
    return exe_path.parent_path().parent_path().parent_path();
}

inline SmokeOptions parse_args(int argc, char** argv, const std::filesystem::path& exe_path) {
    SmokeOptions options;
    const std::filesystem::path repo_root = repo_root_from_exe(exe_path);
    options.model_path = repo_root / "delivery" / "package" / "model" / "best.onnx";
    options.input_dir = repo_root / "python_verify" / "adapter_input";
    options.output_dir = repo_root / "cpp_verify" / "out" / "protocol_preview";

    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--help" || arg == "-h") {
            options.show_help = true;
            continue;
        }
        if (arg == "--model" && index + 1 < argc) {
            options.model_path = argv[++index];
            continue;
        }
        if (arg == "--input-dir" && index + 1 < argc) {
            options.input_dir = argv[++index];
            continue;
        }
        if (arg == "--output-dir" && index + 1 < argc) {
            options.output_dir = argv[++index];
            continue;
        }
    }

    return options;
}

inline std::string format_double(double value) {
    std::ostringstream stream;
    stream << std::setprecision(16) << value;
    return stream.str();
}

inline std::string format_bool(bool value) {
    return value ? "true" : "false";
}

inline void write_binary_file(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

inline void write_lines_file(const std::filesystem::path& path, const std::vector<std::string>& lines) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index > 0) {
            stream << '\n';
        }
        stream << lines[index];
    }
    write_binary_file(path, stream.str());
}

#ifdef CPP_VERIFY_FULL_BUILD

inline std::filesystem::path find_first_image(const std::filesystem::path& input_dir) {
    static const std::vector<std::string> kSupported = {".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff"};
    std::vector<std::filesystem::path> candidates;
    for (const auto& entry : std::filesystem::directory_iterator(input_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string extension = entry.path().extension().string();
        for (const auto& supported : kSupported) {
            if (_stricmp(extension.c_str(), supported.c_str()) == 0) {
                candidates.push_back(entry.path());
                break;
            }
        }
    }
    std::sort(candidates.begin(), candidates.end());
    if (candidates.empty()) {
        throw std::runtime_error("No supported image found in input directory.");
    }
    return candidates.front();
}

inline cv::Mat load_as_cv16u(const std::filesystem::path& image_path) {
    cv::Mat image = cv::imread(image_path.string(), cv::IMREAD_UNCHANGED);
    if (image.empty()) {
        throw std::runtime_error("Failed to read input image.");
    }
    if (image.channels() == 3) {
        cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
    }
    if (image.type() == CV_16UC1) {
        return image;
    }
    if (image.type() == CV_8UC1) {
        cv::Mat promoted16;
        image.convertTo(promoted16, CV_16UC1, 257.0);
        return promoted16;
    }
    throw std::runtime_error("Input image must be grayscale CV_16U or CV_8U.");
}

inline std::vector<std::string> build_init_lines(const std::filesystem::path& model_path) {
    return {
        "",
        model_path.string(),
        ".",
        format_double(1.0),
        format_double(40000.0),
        format_double(32768.0),
        format_double(0.0),
        format_double(0.0),
        format_bool(false),
        format_double(0.0),
        format_double(0.0),
        format_bool(false),
        format_double(0.0),
        format_double(0.0),
        format_bool(false),
        format_double(0.0),
        format_double(0.0),
        format_bool(false),
        format_double(0.0),
        format_double(0.0),
        format_bool(false),
        format_double(0.0),
        format_double(0.0),
        format_bool(false),
        format_double(0.0),
        format_double(0.0),
        format_bool(false),
        format_double(0.0),
        format_double(0.0),
        format_bool(false),
        format_double(0.0),
        format_double(0.0),
        format_bool(false),
        "0",
        "0",
        format_bool(false),
    };
}

inline std::vector<std::string> build_receive_lines() {
    return {
        "CAM01",
        "TEST-QR-0001",
        format_double(90.0),
        format_double(2.5),
        format_double(100.0),
        format_double(1.0),
    };
}

inline void save_output_images(
    const std::filesystem::path& output_dir,
    const std::vector<cv::Mat>& images) {
    static const std::vector<std::string> kNames = {
        "m_drawingImage_8bit.png",
        "m_rawImage_8bit.png",
        "m_enhancedImage_8bit.png",
        "m_segImage.png",
        "m_segImageTo255.png",
    };

    for (std::size_t index = 0; index < kNames.size() && index < images.size(); ++index) {
        cv::imwrite((output_dir / kNames[index]).string(), images[index]);
    }
}

inline int run_full_validation(const SmokeOptions& options) {
    if (!std::filesystem::exists(options.model_path)) {
        std::cout << "ModelPath=" << options.model_path.string() << '\n';
        std::cout << "InputDir=" << options.input_dir.string() << '\n';
        std::cout << "OutputDir=" << options.output_dir.string() << '\n';
        std::cout << "SmokeResult=FAIL" << '\n';
        std::cerr << "Error=Model file does not exist." << '\n';
        return 1;
    }
    if (!std::filesystem::exists(options.input_dir)) {
        std::cout << "ModelPath=" << options.model_path.string() << '\n';
        std::cout << "InputDir=" << options.input_dir.string() << '\n';
        std::cout << "OutputDir=" << options.output_dir.string() << '\n';
        std::cout << "SmokeResult=FAIL" << '\n';
        std::cerr << "Error=Input directory does not exist." << '\n';
        return 1;
    }

    const std::filesystem::path image_path = find_first_image(options.input_dir);
    const cv::Mat image16 = load_as_cv16u(image_path);
    std::filesystem::create_directories(options.output_dir);

    const std::vector<std::string> init_lines = build_init_lines(options.model_path);
    const std::vector<std::string> receive_lines = build_receive_lines();

    std::ostringstream init_stream;
    std::ostringstream receive_stream;
    for (std::size_t index = 0; index < init_lines.size(); ++index) {
        if (index > 0) {
            init_stream << '\n';
        }
        init_stream << init_lines[index];
    }
    for (std::size_t index = 0; index < receive_lines.size(); ++index) {
        if (index > 0) {
            receive_stream << '\n';
        }
        receive_stream << receive_lines[index];
    }

    const std::string init_text = init_stream.str();
    const std::string receive_text = receive_stream.str();
    QByteArray init_raw(init_text.c_str(), static_cast<int>(init_text.size()));
    QByteArray receive_raw(receive_text.c_str(), static_cast<int>(receive_text.size()));

    OH_Detect_Station_Module module;
    if (!module.initParas(init_raw)) {
        std::cerr << "Error=initParas failed." << '\n';
        return 1;
    }

    std::vector<cv::Mat> input_images = {image16};
    if (!module.reciveData(input_images, receive_raw)) {
        std::cerr << "Error=reciveData failed." << '\n';
        return 1;
    }

    if (!module.run()) {
        std::cerr << "Error=run failed." << '\n';
        return 1;
    }

    std::vector<cv::Mat> output_images;
    QByteArray send_raw;
    if (!module.sendData(output_images, send_raw)) {
        std::cerr << "Error=sendData failed." << '\n';
        return 1;
    }

    write_lines_file(options.output_dir / "initParas.bin", init_lines);
    write_lines_file(options.output_dir / "reciveData.bin", receive_lines);
    write_binary_file(options.output_dir / "sendData.bin", send_raw.toStdString());
    save_output_images(options.output_dir, output_images);

    std::cout << "ModelPath=" << options.model_path.string() << '\n';
    std::cout << "InputDir=" << options.input_dir.string() << '\n';
    std::cout << "OutputDir=" << options.output_dir.string() << '\n';
    std::cout << "ImagePath=" << image_path.string() << '\n';
    std::cout << "SmokeResult=PASS" << '\n';
    return 0;
}

#endif

inline int run_smoke(const SmokeOptions& options) {
#ifdef CPP_VERIFY_FULL_BUILD
    return run_full_validation(options);
#else
    const bool model_exists = std::filesystem::exists(options.model_path);
    const bool input_exists = std::filesystem::exists(options.input_dir);

    std::cout << "ModelPath=" << options.model_path.string() << '\n';
    std::cout << "InputDir=" << options.input_dir.string() << '\n';
    std::cout << "OutputDir=" << options.output_dir.string() << '\n';
    std::cout << "ModelExists=" << (model_exists ? "true" : "false") << '\n';
    std::cout << "InputDirExists=" << (input_exists ? "true" : "false") << '\n';

    if (!model_exists || !input_exists) {
        std::cout << "SmokeResult=FAIL" << '\n';
        return 1;
    }

    std::cout << "SmokeResult=PASS" << '\n';
    return 0;
#endif
}

inline void print_help(const std::filesystem::path& exe_path) {
    std::cout << "cpp_verify_smoke" << '\n';
    std::cout << "Usage: " << exe_path.filename().string()
              << " [--model <path>] [--input-dir <path>] [--output-dir <path>] [--help]" << '\n';
}

} // namespace cpp_verify

#endif // CPP_VERIFY_SMOKE_RUNNER_H

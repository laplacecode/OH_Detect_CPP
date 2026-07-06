#include "../include/smoke_runner.h"

#include <windows.h>

#include <exception>
#include <sstream>

namespace {

using CreateModuleFn = OH_Detect_Station_Module* (*)();
using DestroyModuleFn = void (*)(OH_Detect_Station_Module*);

struct DllSmokeOptions {
    bool show_help = false;
    std::filesystem::path module_path;
    std::filesystem::path model_path;
    std::filesystem::path input_dir;
    std::filesystem::path output_dir;
};

std::string last_windows_error() {
    const DWORD error_code = GetLastError();
    if (error_code == 0) {
        return "unknown error";
    }

    LPSTR message_buffer = nullptr;
    const DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&message_buffer),
        0,
        nullptr);

    std::string message = size > 0 && message_buffer != nullptr ? message_buffer : "unknown error";
    if (message_buffer != nullptr) {
        LocalFree(message_buffer);
    }
    return message;
}

DllSmokeOptions parse_dll_args(int argc, char** argv, const std::filesystem::path& exe_path) {
    DllSmokeOptions options;
    const std::filesystem::path exe_dir = exe_path.parent_path();
    options.module_path = exe_dir / "Hd_AI_Station_Jr_module.dll";
    options.model_path = exe_dir / "model" / "best.onnx";
    options.input_dir = exe_dir / "input";
    options.output_dir = exe_dir / "output";

    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--help" || arg == "-h") {
            options.show_help = true;
            continue;
        }
        if (arg == "--module" && index + 1 < argc) {
            options.module_path = argv[++index];
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

void print_dll_help(const std::filesystem::path& exe_path) {
    std::cout << "verify_dll_smoke" << '\n';
    std::cout << "Usage: " << exe_path.filename().string()
              << " [--module <path>] [--model <path>] [--input-dir <path>] [--output-dir <path>] [--help]" << '\n';
}

void ensure_exists(const std::filesystem::path& path, const char* label) {
    if (!std::filesystem::exists(path)) {
        std::ostringstream message;
        message << label << " does not exist: " << path.string();
        throw std::runtime_error(message.str());
    }
}

int run_dll_smoke(const DllSmokeOptions& options) {
    ensure_exists(options.module_path, "Module DLL");
    ensure_exists(options.model_path, "Model file");
    ensure_exists(options.input_dir, "Input directory");

    HMODULE module_handle = LoadLibraryW(options.module_path.wstring().c_str());
    if (module_handle == nullptr) {
        throw std::runtime_error("LoadLibrary failed: " + last_windows_error());
    }

    auto create_fn = reinterpret_cast<CreateModuleFn>(GetProcAddress(module_handle, "create"));
    auto destroy_fn = reinterpret_cast<DestroyModuleFn>(GetProcAddress(module_handle, "destory"));
    if (destroy_fn == nullptr) {
        destroy_fn = reinterpret_cast<DestroyModuleFn>(GetProcAddress(module_handle, "destroy"));
    }

    if (create_fn == nullptr || destroy_fn == nullptr) {
        FreeLibrary(module_handle);
        throw std::runtime_error("Module does not export create/destory.");
    }

    OH_Detect_Station_Module* module = create_fn();
    if (module == nullptr) {
        FreeLibrary(module_handle);
        throw std::runtime_error("create returned null.");
    }

    try {
        const std::filesystem::path image_path = cpp_verify::find_first_image(options.input_dir);
        const cv::Mat image16 = cpp_verify::load_as_cv16u(image_path);
        std::filesystem::create_directories(options.output_dir);

        const std::vector<std::string> init_lines = cpp_verify::build_init_lines(options.model_path);
        const std::vector<std::string> receive_lines = cpp_verify::build_receive_lines();

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

        if (!module->initParas(init_raw)) {
            throw std::runtime_error("initParas failed.");
        }

        std::vector<cv::Mat> input_images = {image16};
        if (!module->reciveData(input_images, receive_raw)) {
            throw std::runtime_error("reciveData failed.");
        }

        if (!module->run()) {
            throw std::runtime_error("run failed.");
        }

        std::vector<cv::Mat> output_images;
        QByteArray send_raw;
        if (!module->sendData(output_images, send_raw)) {
            throw std::runtime_error("sendData failed.");
        }

        cpp_verify::write_lines_file(options.output_dir / "initParas.bin", init_lines);
        cpp_verify::write_lines_file(options.output_dir / "reciveData.bin", receive_lines);
        cpp_verify::write_binary_file(options.output_dir / "sendData.bin", send_raw.toStdString());
        cpp_verify::save_output_images(options.output_dir, output_images);

        std::cout << "ModulePath=" << options.module_path.string() << '\n';
        std::cout << "ModelPath=" << options.model_path.string() << '\n';
        std::cout << "InputDir=" << options.input_dir.string() << '\n';
        std::cout << "OutputDir=" << options.output_dir.string() << '\n';
        std::cout << "ImagePath=" << image_path.string() << '\n';
        std::cout << "DllSmokeResult=PASS" << '\n';

        destroy_fn(module);
        FreeLibrary(module_handle);
        return 0;
    } catch (...) {
        destroy_fn(module);
        FreeLibrary(module_handle);
        throw;
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::filesystem::path exe_path = std::filesystem::absolute(argv[0]);
        const DllSmokeOptions options = parse_dll_args(argc, argv, exe_path);
        if (options.show_help) {
            print_dll_help(exe_path);
            return 0;
        }
        return run_dll_smoke(options);
    } catch (const std::exception& ex) {
        std::cerr << "DllSmokeResult=FAIL" << '\n';
        std::cerr << "Error=" << ex.what() << '\n';
        return 1;
    }
}
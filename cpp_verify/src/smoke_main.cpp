#include "../include/smoke_runner.h"

#include <exception>

int main(int argc, char** argv) {
    try {
        const std::filesystem::path exe_path = std::filesystem::absolute(argv[0]);
        const cpp_verify::SmokeOptions options = cpp_verify::parse_args(argc, argv, exe_path);
        if (options.show_help) {
            cpp_verify::print_help(exe_path);
            return 0;
        }
        return cpp_verify::run_smoke(options);
    } catch (const std::exception& ex) {
        std::cerr << "SmokeResult=FAIL" << '\n';
        std::cerr << "Error=" << ex.what() << '\n';
        return 1;
    }
}

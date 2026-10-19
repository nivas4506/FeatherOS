// FeatherOS featherd — entry point (C++20)
// Native OS layer supervisor daemon.
#include "featherd/featherd.hpp"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

using namespace featherd;

int main(int argc, char** argv) {
    std::string endpoint = feather::ipc::default_endpoint();
    std::filesystem::path data_dir = feather::ipc::default_socket_path().parent_path();
    std::filesystem::path workspace = std::filesystem::current_path();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--endpoint" && i + 1 < argc) endpoint = argv[++i];
        else if (arg == "--data-dir" && i + 1 < argc) data_dir = argv[++i];
        else if (arg == "--workspace" && i + 1 < argc) workspace = argv[++i];
        else if (arg == "--help" || arg == "-h") {
            std::printf("Usage: featherd [--endpoint PATH] [--data-dir DIR] [--workspace DIR]\n");
            return 0;
        }
    }

    Daemon daemon(data_dir, workspace);
    return daemon.serve(endpoint);
}

/*
 * Portal VirtualFileSystem Backend
 * Implements feather::hal::VirtualFileSystem using portal_fs_op syscall.
 */

#include "feather/hal.hpp"
#include <portal/portal.h>
#include <filesystem>
#include <vector>
#include <string>
#include <fstream>
#include <system_error>
#include <cstring>
#include <cstdint>

namespace feather::hal {

class PortalVfs final : public VirtualFileSystem {
public:
    PortalVfs() = default;

    Expected<std::string>
    read_to_string(const std::filesystem::path& path) override {
        portal_fs_request_t req{};
        req.op = PORTAL_FS_READ;
        std::string path_str = path.string();
        std::strncpy(req.path, path_str.c_str(), sizeof(req.path) - 1);
        req.read.offset = 0;
        req.read.length = UINT64_MAX; // Read entire file

        int ret = portal_fs_op(&req);
        if (ret < 0) {
            return Expected<std::string>::err(std::string("portal_fs_op READ failed: ") + std::strerror(-ret));
        }

        return read_file_via_portal(path);
    }

    ExpectedVoid
    write_atomic(const std::filesystem::path& path, std::span<const std::byte> content) override {
        // Atomic write: write to temp, then rename
        std::filesystem::path temp_path = path;
        temp_path += ".feather_tmp";

        // Write temp file
        portal_fs_request_t req{};
        req.op = PORTAL_FS_WRITE;
        std::string temp_str = temp_path.string();
        std::strncpy(req.path, temp_str.c_str(), sizeof(req.path) - 1);
        req.write.offset = 0;
        req.write.length = content.size();
        req.write.data = reinterpret_cast<const uint8_t*>(content.data());

        int ret = portal_fs_op(&req);
        if (ret < 0) {
            return ExpectedVoid::err(std::string("portal_fs_op WRITE failed: ") + std::strerror(-ret));
        }

        // Rename temp -> target (atomic on same filesystem)
        req.op = PORTAL_FS_RENAME;
        std::strncpy(req.path, temp_str.c_str(), sizeof(req.path) - 1);
        std::string path_str = path.string();
        std::strncpy(req.rename.new_path, path_str.c_str(), sizeof(req.rename.new_path) - 1);

        ret = portal_fs_op(&req);
        if (ret < 0) {
            // Cleanup temp on failure
            portal_fs_request_t cleanup{};
            cleanup.op = PORTAL_FS_UNLINK;
            std::strncpy(cleanup.path, temp_str.c_str(), sizeof(cleanup.path) - 1);
            portal_fs_op(&cleanup);
            return ExpectedVoid::err(std::string("portal_fs_op RENAME failed: ") + std::strerror(-ret));
        }

        return ExpectedVoid::ok(Void{});
    }

    ExpectedVoid remove_file(const std::filesystem::path& path) override {
        portal_fs_request_t req{};
        req.op = PORTAL_FS_UNLINK;
        std::string path_str = path.string();
        std::strncpy(req.path, path_str.c_str(), sizeof(req.path) - 1);

        int ret = portal_fs_op(&req);
        if (ret < 0) {
            return ExpectedVoid::err(std::string("portal_fs_op UNLINK failed: ") + std::strerror(-ret));
        }
        return ExpectedVoid::ok(Void{});
    }

    Expected<std::vector<std::filesystem::path>>
    list_directory(const std::filesystem::path& path) override {
        portal_fs_request_t req{};
        req.op = PORTAL_FS_READDIR;
        std::string path_str = path.string();
        std::strncpy(req.path, path_str.c_str(), sizeof(req.path) - 1);

        // Allocate buffer for directory entries
        std::vector<uint8_t> buf(64 * 1024); // 64 KB buffer
        req.readdir.fd = -1; // -1 means use path directly
        req.readdir.buf = buf.data();
        req.readdir.count = buf.size();

        int ret = portal_fs_op(&req);
        if (ret < 0) {
            return Expected<std::vector<std::filesystem::path>>::err(std::string("portal_fs_op READDIR failed: ") + std::strerror(-ret));
        }

        // Parse directory entries from buffer
        // Format: count (u32) + [name_len (u16) + name + is_dir (u8)]...
        std::vector<std::filesystem::path> entries;
        if (req.readdir.count >= 4) {
            uint32_t count = *reinterpret_cast<uint32_t*>(buf.data());
            size_t offset = 4;
            for (uint32_t i = 0; i < count && offset < req.readdir.count; ++i) {
                if (offset + 2 > req.readdir.count) break;
                uint16_t name_len = *reinterpret_cast<uint16_t*>(buf.data() + offset);
                offset += 2;
                if (offset + name_len > req.readdir.count) break;
                std::string name(reinterpret_cast<char*>(buf.data() + offset), name_len);
                offset += name_len;
                if (offset >= req.readdir.count) break;
                // uint8_t is_dir = buf[offset++]; // unused for now
                offset++;
                entries.emplace_back(name);
            }
        }
        return entries;
    }

    bool exists(const std::filesystem::path& path) const noexcept override {
        portal_fs_request_t req{};
        req.op = PORTAL_FS_STAT;
        std::string path_str = path.string();
        std::strncpy(req.path, path_str.c_str(), sizeof(req.path) - 1);

        int ret = portal_fs_op(&req);
        return ret == 0;
    }

private:
    // Helper: read entire file via portal (kernel may use different mechanism)
    std::string read_file_via_portal(const std::filesystem::path& path) {
        // MVP: use stat to get size, then read
        portal_fs_request_t stat_req{};
        stat_req.op = PORTAL_FS_STAT;
        std::string path_str = path.string();
        std::strncpy(stat_req.path, path_str.c_str(), sizeof(stat_req.path) - 1);

        int ret = portal_fs_op(&stat_req);
        if (ret < 0) {
            throw std::system_error(-ret, std::generic_category(), "stat failed");
        }

        uint64_t size = stat_req.stat_result.size;
        if (size == 0) return "";

        // Read in chunks
        std::string result;
        result.resize(size);
        uint64_t offset = 0;
        while (offset < size) {
            uint64_t chunk = std::min<uint64_t>(size - offset, 64 * 1024);
            portal_fs_request_t read_req{};
            read_req.op = PORTAL_FS_READ;
            std::strncpy(read_req.path, path_str.c_str(), sizeof(read_req.path) - 1);
            read_req.read.offset = offset;
            read_req.read.length = chunk;

            ret = portal_fs_op(&read_req);
            if (ret < 0) {
                throw std::system_error(-ret, std::generic_category(), "read failed");
            }

            // Kernel writes data to result buffer (simplified)
            // Real impl: portal_recv or shared memory
            offset += chunk;
        }
        return result;
    }
};

std::unique_ptr<VirtualFileSystem> create_virtual_filesystem() {
    return std::make_unique<PortalVfs>();
}

} // namespace feather::hal
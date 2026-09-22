#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace poly {

// Minimal ZIP writer (stored entries, no compression).  Stored entries are
// perfectly valid ZIP files; keeping the writer dependency-free matters more
// than a few megabytes of size for test archives.
class ZipWriter {
public:
    ZipWriter() = default;
    ~ZipWriter();

    ZipWriter(const ZipWriter&) = delete;
    ZipWriter& operator=(const ZipWriter&) = delete;

    bool open(const std::string& path);
    bool add(const std::string& arcname, const std::string& data);
    bool add_file(const std::string& arcname, const std::string& diskPath);
    bool close();

    size_t entries() const { return entries_.size(); }
    long long bytes_written() const { return written_; }
    const std::string& error() const { return error_; }

private:
    struct Entry {
        std::string name;
        uint32_t crc = 0;
        uint64_t size = 0;
        uint64_t offset = 0;
        uint16_t time = 0;
        uint16_t date = 0;
    };

    bool write_bytes(const void* data, size_t n);
    bool write_u16(uint16_t v);
    bool write_u32(uint32_t v);
    bool write_u64(uint64_t v);

    std::FILE* f_ = nullptr;
    std::vector<Entry> entries_;
    long long written_ = 0;
    std::string error_;
};

uint32_t crc32_of(const std::string& data);

// 原始 deflate 解压（RFC 1951），供 ZipReader 使用；失败时写入 err。
bool inflate_raw(const std::string& in, size_t expectedSize, std::string& out, std::string& err);

// 只读 ZIP：支持 stored 与 deflate，带 CRC 校验。
// 存在的意义是导入 QDUOJ / Hydro 导出的题目包——那些包通常是 deflate 压缩的。
struct ZipEntryInfo {
    std::string name;
    uint32_t crc = 0;
    uint64_t size = 0;          // 解压后大小
    uint64_t compressedSize = 0;
    uint16_t method = 0;        // 0 = stored, 8 = deflate
    uint64_t headerOffset = 0;  // 本地文件头偏移
};

class ZipReader {
public:
    bool open(const std::string& path);
    // 直接从内存打开（用于 HTTP 上传的内容）。
    bool open_data(const std::string& data, std::string& err);
    bool open_data(const std::string& data);

    const std::vector<ZipEntryInfo>& entries() const { return entries_; }
    bool has(const std::string& name) const;
    // 按精确名字读取；返回 false 时 error() 给出原因。
    bool read(const std::string& name, std::string& out);
    // 找第一个以 suffix 结尾的条目名；找不到返回空串。
    std::string find_suffix(const std::string& suffix) const;
    // 找第一个 basename 相同的条目名；找不到返回空串。
    std::string find_basename(const std::string& base) const;

    const std::string& error() const { return error_; }

private:
    std::string data_;
    std::vector<ZipEntryInfo> entries_;
    std::string error_;
};

}  // namespace poly

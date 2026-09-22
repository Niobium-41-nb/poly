#include "zip.h"

#include <algorithm>
#include <cstring>
#include <ctime>

#include "format.h"
#include "fsutil.h"
#include "plat.h"
#include "strutil.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace poly {

namespace {

uint32_t crc_table[256];
bool crc_ready = false;

void init_crc() {
    if (crc_ready) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table[i] = c;
    }
    crc_ready = true;
}

void dos_time(std::time_t t, uint16_t& dosDate, uint16_t& dosTime) {
    struct tm tmv;
#ifdef _WIN32
    if (localtime_s(&tmv, &t) != 0) memset(&tmv, 0, sizeof(tmv));
#else
    localtime_r(&t, &tmv);
#endif
    int year = tmv.tm_year + 1900;
    if (year < 1980) year = 1980;
    dosDate = static_cast<uint16_t>(((year - 1980) << 9) | ((tmv.tm_mon + 1) << 5) | tmv.tm_mday);
    dosTime = static_cast<uint16_t>((tmv.tm_hour << 11) | (tmv.tm_min << 5) | (tmv.tm_sec / 2));
}

}  // namespace

uint32_t crc32_of(const std::string& data) {
    init_crc();
    uint32_t c = 0xFFFFFFFFu;
    for (unsigned char ch : data) c = crc_table[(c ^ ch) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

ZipWriter::~ZipWriter() {
    if (f_) {
        std::fclose(f_);
        f_ = nullptr;
    }
}

bool ZipWriter::open(const std::string& path) {
    std::string dir = fs::dirname(path);
    if (!dir.empty() && dir != ".") fs::mkdirs(dir);
#ifdef _WIN32
    f_ = _wfopen(to_wide(path).c_str(), L"wb");
#else
    f_ = std::fopen(path.c_str(), "wb");
#endif
    if (!f_) {
        error_ = str("无法创建 {}", path);
        return false;
    }
    return true;
}

bool ZipWriter::write_bytes(const void* data, size_t n) {
    if (!f_) {
        error_ = "zip 未打开";
        return false;
    }
    if (n == 0) return true;
    if (std::fwrite(data, 1, n, f_) != n) {
        error_ = "写入 zip 失败（磁盘空间不足？）";
        return false;
    }
    written_ += static_cast<long long>(n);
    return true;
}

bool ZipWriter::write_u16(uint16_t v) {
    unsigned char buf[2];
    buf[0] = static_cast<unsigned char>(v & 0xFF);
    buf[1] = static_cast<unsigned char>((v >> 8) & 0xFF);
    return write_bytes(buf, 2);
}

bool ZipWriter::write_u32(uint32_t v) {
    unsigned char buf[4];
    for (int i = 0; i < 4; i++) buf[i] = static_cast<unsigned char>((v >> (8 * i)) & 0xFF);
    return write_bytes(buf, 4);
}

bool ZipWriter::write_u64(uint64_t v) {
    unsigned char buf[8];
    for (int i = 0; i < 8; i++) buf[i] = static_cast<unsigned char>((v >> (8 * i)) & 0xFF);
    return write_bytes(buf, 8);
}

bool ZipWriter::add(const std::string& arcname, const std::string& data) {
    if (!f_) {
        error_ = "zip 未打开";
        return false;
    }
    Entry e;
    e.name = arcname;
    for (char& c : e.name)
        if (c == '\\') c = '/';
    e.crc = crc32_of(data);
    e.size = data.size();
    e.offset = static_cast<uint64_t>(written_);
    dos_time(std::time(nullptr), e.date, e.time);

    if (!write_u32(0x04034b50u)) return false;
    if (!write_u16(20)) return false;
    if (!write_u16(0)) return false;
    if (!write_u16(0)) return false;
    if (!write_u16(e.time)) return false;
    if (!write_u16(e.date)) return false;
    if (!write_u32(e.crc)) return false;
    if (!write_u32(static_cast<uint32_t>(e.size))) return false;
    if (!write_u32(static_cast<uint32_t>(e.size))) return false;
    if (!write_u16(static_cast<uint16_t>(e.name.size()))) return false;
    if (!write_u16(0)) return false;
    if (!write_bytes(e.name.data(), e.name.size())) return false;
    if (!write_bytes(data.data(), data.size())) return false;

    entries_.push_back(e);
    return true;
}

bool ZipWriter::add_file(const std::string& arcname, const std::string& diskPath) {
    std::string data;
    if (!fs::read_file(diskPath, data)) {
        error_ = str("无法读取 {}", diskPath);
        return false;
    }
    return add(arcname, data);
}

bool ZipWriter::close() {
    if (!f_) return false;
    uint64_t cdStart = static_cast<uint64_t>(written_);
    for (const Entry& e : entries_) {
        if (!write_u32(0x02014b50u)) return false;
        if (!write_u16(20)) return false;
        if (!write_u16(20)) return false;
        if (!write_u16(0)) return false;
        if (!write_u16(0)) return false;
        if (!write_u16(e.time)) return false;
        if (!write_u16(e.date)) return false;
        if (!write_u32(e.crc)) return false;
        if (!write_u32(static_cast<uint32_t>(e.size))) return false;
        if (!write_u32(static_cast<uint32_t>(e.size))) return false;
        if (!write_u16(static_cast<uint16_t>(e.name.size()))) return false;
        if (!write_u16(0)) return false;
        if (!write_u16(0)) return false;
        if (!write_u16(0)) return false;
        if (!write_u16(0)) return false;
        if (!write_u32(0)) return false;
        if (!write_u32(static_cast<uint32_t>(e.offset))) return false;
        if (!write_bytes(e.name.data(), e.name.size())) return false;
    }
    uint64_t cdSize = static_cast<uint64_t>(written_) - cdStart;

    if (!write_u32(0x06054b50u)) return false;
    if (!write_u16(0)) return false;
    if (!write_u16(0)) return false;
    if (!write_u16(static_cast<uint16_t>(entries_.size()))) return false;
    if (!write_u16(static_cast<uint16_t>(entries_.size()))) return false;
    if (!write_u32(static_cast<uint32_t>(cdSize))) return false;
    if (!write_u32(static_cast<uint32_t>(cdStart))) return false;
    if (!write_u16(0)) return false;

    std::fclose(f_);
    f_ = nullptr;
    return true;
}

// ============================================================== 原始 deflate 解压
//
// 独立实现（思路与 zlib 的 puff.c 相同：先把码长表展开成「计数 + 偏移」，再逐位读码）。
// 只为了能读 QDUOJ / Hydro 导出的题目包——它们通常是 deflate 压缩的。

namespace {

class BitReader {
public:
    explicit BitReader(const std::string& in) : in_(in) {}

    // 读 n 位（n <= 24），LSB 优先。
    bool bits(int n, uint32_t& out) {
        while (count_ < n) {
            if (pos_ >= in_.size()) return false;
            buf_ |= static_cast<uint32_t>(static_cast<unsigned char>(in_[pos_++])) << count_;
            count_ += 8;
        }
        out = buf_ & ((1u << n) - 1u);
        buf_ >>= n;
        count_ -= n;
        return true;
    }

    // 丢弃当前字节里剩余的位（stored 块需要）。
    void align() {
        buf_ = 0;
        count_ = 0;
    }

    // 逐字节取 n 个字节（只能在 align() 之后调用）。
    bool take(size_t n, std::string& out) {
        if (pos_ + n > in_.size()) return false;
        out.append(in_, pos_, n);
        pos_ += n;
        return true;
    }

private:
    const std::string& in_;
    size_t pos_ = 0;
    uint32_t buf_ = 0;
    int count_ = 0;
};

const int kMaxBits = 15;
const int kNumSymbols = 288;  // 字面/长度码最多 288 个
const int kMaxLens = 320;     // 动态块的 lencode + distcode 码长数组长度

struct Huffman {
    int count[kMaxBits + 1] = {0};
    int symbol[kNumSymbols] = {0};
};

// 建表。返回负值表示码表溢出（非法），正值表示码表不完整（仍可解码）。
int huffman_build(Huffman& h, const int* lengths, int n) {
    for (int i = 0; i <= kMaxBits; i++) h.count[i] = 0;
    for (int i = 0; i < n; i++) h.count[lengths[i]]++;
    int left = 1;
    for (int len = 1; len <= kMaxBits; len++) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0) return left;
    }
    int offs[kMaxBits + 1];
    offs[1] = 0;
    for (int len = 1; len < kMaxBits; len++) offs[len + 1] = offs[len] + h.count[len];
    for (int i = 0; i < n; i++) {
        if (lengths[i]) h.symbol[offs[lengths[i]]++] = i;
    }
    return left;
}

// 解出一个码字，失败返回 -1。
int huffman_decode(BitReader& br, const Huffman& h) {
    int code = 0;
    int first = 0;
    int index = 0;
    for (int len = 1; len <= kMaxBits; len++) {
        uint32_t bit = 0;
        if (!br.bits(1, bit)) return -1;
        code |= static_cast<int>(bit);
        int count = h.count[len];
        if (code - count < first) return h.symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

const int kLenBase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                          31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const int kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                           2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const int kDistBase[30] = {1,    2,    3,    4,    5,    7,     9,     13,    17,   25,
                           33,   49,   65,   97,   129,  193,   257,   385,   513,  769,
                           1025, 1537, 2049, 3073, 4097, 6145,  8193,  12289, 16385, 24577};
const int kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                            6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
const int kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

void build_fixed(Huffman& lencode, Huffman& distcode) {
    int lengths[kNumSymbols];
    int symbol = 0;
    for (; symbol < 144; symbol++) lengths[symbol] = 8;
    for (; symbol < 256; symbol++) lengths[symbol] = 9;
    for (; symbol < 280; symbol++) lengths[symbol] = 7;
    for (; symbol < 288; symbol++) lengths[symbol] = 8;
    huffman_build(lencode, lengths, 288);
    for (int i = 0; i < 30; i++) lengths[i] = 5;
    huffman_build(distcode, lengths, 30);
}

// 用 lencode 读 n 个码长（处理 16/17/18 三种重复编码）。
bool read_lengths(BitReader& br, const Huffman& lencode, int n, int* lengths) {
    int index = 0;
    while (index < n) {
        int symbol = huffman_decode(br, lencode);
        if (symbol < 0) return false;
        if (symbol < 16) {
            lengths[index++] = symbol;
            continue;
        }
        int repeat = 0;
        int value = 0;
        uint32_t bits = 0;
        if (symbol == 16) {  // 重复上一个码长 3..6 次
            if (index == 0) return false;
            if (!br.bits(2, bits)) return false;
            repeat = 3 + static_cast<int>(bits);
            value = lengths[index - 1];
        } else if (symbol == 17) {  // 重复 0，3..10 次
            if (!br.bits(3, bits)) return false;
            repeat = 3 + static_cast<int>(bits);
        } else if (symbol == 18) {  // 重复 0，11..138 次
            if (!br.bits(7, bits)) return false;
            repeat = 11 + static_cast<int>(bits);
        } else {
            return false;
        }
        if (index + repeat > n) return false;
        while (repeat-- > 0) lengths[index++] = value;
    }
    return true;
}

bool inflate_block(BitReader& br, const Huffman& lencode, const Huffman& distcode, std::string& out,
                   std::string& err) {
    for (;;) {
        int symbol = huffman_decode(br, lencode);
        if (symbol < 0) {
            err = "Huffman 解码失败";
            return false;
        }
        if (symbol < 256) {
            out.push_back(static_cast<char>(symbol));
            continue;
        }
        if (symbol == 256) return true;  // 块结束
        symbol -= 257;
        if (symbol >= 29) {
            err = "非法的长度码";
            return false;
        }
        uint32_t bits = 0;
        if (kLenExtra[symbol] && !br.bits(kLenExtra[symbol], bits)) {
            err = "读取匹配长度失败";
            return false;
        }
        int length = kLenBase[symbol] + static_cast<int>(bits);

        int dsymbol = huffman_decode(br, distcode);
        if (dsymbol < 0) {
            err = "Huffman 解码失败（距离码）";
            return false;
        }
        if (dsymbol >= 30) {
            err = "非法的距离码";
            return false;
        }
        bits = 0;
        if (kDistExtra[dsymbol] && !br.bits(kDistExtra[dsymbol], bits)) {
            err = "读取匹配距离失败";
            return false;
        }
        size_t distance = static_cast<size_t>(kDistBase[dsymbol]) + bits;
        if (distance > out.size()) {
            err = "匹配距离超出已解压数据";
            return false;
        }
        size_t start = out.size() - distance;
        for (int i = 0; i < length; i++) out.push_back(out[start + i]);  // 允许重叠
    }
}

}  // namespace

bool inflate_raw(const std::string& in, size_t expectedSize, std::string& out, std::string& err) {
    const size_t kMaxOutput = static_cast<size_t>(1024) * 1024 * 1024;  // 1 GiB 上限
    BitReader br(in);
    out.clear();
    out.reserve(expectedSize ? expectedSize : std::min<size_t>(in.size() * 4, kMaxOutput));

    bool last = false;
    while (!last) {
        uint32_t finalBlock = 0;
        uint32_t type = 0;
        if (!br.bits(1, finalBlock) || !br.bits(2, type)) {
            err = "deflate 数据提前结束";
            return false;
        }
        last = finalBlock != 0;
        if (type == 0) {  // stored
            br.align();
            uint32_t len = 0;
            uint32_t nlen = 0;
            if (!br.bits(16, len) || !br.bits(16, nlen)) {
                err = "读取 stored 块头失败";
                return false;
            }
            if ((len ^ 0xFFFFu) != nlen) {
                err = "stored 块长度校验失败";
                return false;
            }
            if (!br.take(len, out)) {
                err = "stored 块数据不足";
                return false;
            }
        } else if (type == 1) {  // 固定 Huffman
            Huffman lencode;
            Huffman distcode;
            build_fixed(lencode, distcode);
            if (!inflate_block(br, lencode, distcode, out, err)) return false;
        } else if (type == 2) {  // 动态 Huffman
            uint32_t hlit = 0;
            uint32_t hdist = 0;
            uint32_t hclen = 0;
            if (!br.bits(5, hlit) || !br.bits(5, hdist) || !br.bits(4, hclen)) {
                err = "读取动态块头失败";
                return false;
            }
            int nlen = 257 + static_cast<int>(hlit);
            int ndist = 1 + static_cast<int>(hdist);
            int ncode = 4 + static_cast<int>(hclen);
            if (nlen > 286 || ndist > 30) {
                err = "动态块码表尺寸非法";
                return false;
            }
            int lengths[kMaxLens];
            for (int i = 0; i < kMaxLens; i++) lengths[i] = 0;
            for (int i = 0; i < ncode; i++) {
                uint32_t v = 0;
                if (!br.bits(3, v)) {
                    err = "读取码长码失败";
                    return false;
                }
                lengths[kOrder[i]] = static_cast<int>(v);
            }
            Huffman lencode;
            if (huffman_build(lencode, lengths, 19) < 0) {
                err = "码长码表非法";
                return false;
            }
            int all[kMaxLens];
            for (int i = 0; i < kMaxLens; i++) all[i] = 0;
            if (!read_lengths(br, lencode, nlen + ndist, all)) {
                err = "读取码长失败";
                return false;
            }
            if (all[256] == 0) {
                err = "缺少块结束码";
                return false;
            }
            Huffman lencode2;
            Huffman distcode;
            if (huffman_build(lencode2, all, nlen) < 0) {
                err = "字面码表非法";
                return false;
            }
            if (huffman_build(distcode, all + nlen, ndist) < 0) {
                err = "距离码表非法";
                return false;
            }
            if (!inflate_block(br, lencode2, distcode, out, err)) return false;
        } else {
            err = "不支持的 deflate 块类型 3";
            return false;
        }
        if (out.size() > kMaxOutput) {
            err = "解压结果过大";
            return false;
        }
    }
    if (expectedSize != 0 && out.size() != expectedSize) {
        err = str("解压得到 {} 字节，压缩包头声明 {}", out.size(), expectedSize);
        return false;
    }
    return true;
}

// ============================================================== 只读 ZIP

namespace {

uint16_t rd_u16(const std::string& d, size_t off) {
    if (off + 2 > d.size()) return 0;
    return static_cast<uint16_t>(static_cast<unsigned char>(d[off]) |
                                 (static_cast<unsigned char>(d[off + 1]) << 8));
}

uint32_t rd_u32(const std::string& d, size_t off) {
    if (off + 4 > d.size()) return 0;
    return static_cast<uint32_t>(static_cast<unsigned char>(d[off])) |
           (static_cast<uint32_t>(static_cast<unsigned char>(d[off + 1])) << 8) |
           (static_cast<uint32_t>(static_cast<unsigned char>(d[off + 2])) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(d[off + 3])) << 24);
}

uint64_t rd_u64(const std::string& d, size_t off) {
    return static_cast<uint64_t>(rd_u32(d, off)) | (static_cast<uint64_t>(rd_u32(d, off + 4)) << 32);
}

const uint32_t kEocdSig = 0x06054b50u;
const uint32_t kEocd64LocSig = 0x07064b50u;
const uint32_t kEocd64Sig = 0x06064b50u;
const uint32_t kCentralSig = 0x02014b50u;
const uint32_t kLocalSig = 0x04034b50u;

// 从 ZIP64 扩展字段里补齐被写成 0xFFFFFFFF 的字段。
void apply_zip64_extra(const std::string& d, size_t off, uint16_t extraLen, ZipEntryInfo& e) {
    size_t p = off;
    size_t end = off + extraLen;
    while (p + 4 <= end && p + 4 <= d.size()) {
        uint16_t tag = rd_u16(d, p);
        uint16_t size = rd_u16(d, p + 2);
        size_t body = p + 4;
        if (tag == 0x0001) {
            size_t q = body;
            if (e.size == 0xFFFFFFFFu && q + 8 <= d.size()) {
                e.size = rd_u64(d, q);
                q += 8;
            }
            if (e.compressedSize == 0xFFFFFFFFu && q + 8 <= d.size()) {
                e.compressedSize = rd_u64(d, q);
                q += 8;
            }
            if (e.headerOffset == 0xFFFFFFFFu && q + 8 <= d.size()) {
                e.headerOffset = rd_u64(d, q);
            }
            return;
        }
        p = body + size;
    }
}

// 规范化条目名：统一分隔符、去掉开头的 "./"。
std::string normalize_entry_name(const std::string& name) {
    std::string out = name;
    for (char& c : out) {
        if (c == '\\') c = '/';
    }
    while (out.size() >= 2 && out[0] == '.' && out[1] == '/') out.erase(0, 2);
    return out;
}

bool zip_read_entry(const std::string& data, const ZipEntryInfo& e, std::string& out,
                    std::string& error) {
    size_t p = static_cast<size_t>(e.headerOffset);
    if (p + 30 > data.size() || rd_u32(data, p) != kLocalSig) {
        error = str("{}：本地文件头损坏", e.name);
        return false;
    }
    uint16_t nameLen = rd_u16(data, p + 26);
    uint16_t extraLen = rd_u16(data, p + 28);
    size_t dataOff = p + 30 + static_cast<size_t>(nameLen) + static_cast<size_t>(extraLen);
    if (dataOff > data.size() || e.compressedSize > data.size() - dataOff) {
        error = str("{}：压缩数据越界（zip 可能不完整）", e.name);
        return false;
    }
    std::string raw = data.substr(dataOff, static_cast<size_t>(e.compressedSize));
    if (e.method == 0) {
        out = std::move(raw);
    } else if (e.method == 8) {
        std::string err;
        if (!inflate_raw(raw, static_cast<size_t>(e.size), out, err)) {
            error = str("{}：{}", e.name, err);
            return false;
        }
    } else {
        error = str("{}：不支持的压缩方式 {}", e.name, e.method);
        return false;
    }
    if (e.crc != 0 && crc32_of(out) != e.crc) {
        error = str("{}：CRC 校验失败", e.name);
        return false;
    }
    return true;
}

}  // namespace

bool ZipReader::open(const std::string& path) {
    std::string data;
    if (!fs::read_file(path, data)) {
        error_ = str("无法读取 {}", path);
        return false;
    }
    return open_data(data, error_);
}

bool ZipReader::open_data(const std::string& data) {
    std::string err;
    return open_data(data, err);
}

bool ZipReader::open_data(const std::string& data, std::string& err) {
    data_ = data;
    entries_.clear();
    error_.clear();
    if (data_.size() < 22) {
        error_ = "文件太小，不是有效的 zip";
        err = error_;
        return false;
    }
    // 从尾部向前找中央目录结束记录（最多回退 64 KiB + 22）
    size_t eocd = std::string::npos;
    size_t lo = data_.size() > 65557 ? data_.size() - 65557 : 0;
    for (size_t i = data_.size() - 21; i-- > lo;) {
        if (rd_u32(data_, i) == kEocdSig) {
            eocd = i;
            break;
        }
    }
    if (eocd == std::string::npos) {
        error_ = "找不到 zip 中央目录结束记录";
        err = error_;
        return false;
    }
    uint64_t count = rd_u16(data_, eocd + 10);
    uint64_t cdOffset = rd_u32(data_, eocd + 16);

    // ZIP64（大包）兜底
    if (eocd >= 20 && rd_u32(data_, eocd - 20) == kEocd64LocSig) {
        uint64_t rec = rd_u64(data_, eocd - 20 + 8);
        if (rec + 56 <= data_.size() && rd_u32(data_, static_cast<size_t>(rec)) == kEocd64Sig) {
            count = rd_u64(data_, static_cast<size_t>(rec) + 32);
            cdOffset = rd_u64(data_, static_cast<size_t>(rec) + 48);
        }
    }

    size_t p = static_cast<size_t>(cdOffset);
    for (uint64_t i = 0; i < count; i++) {
        if (p + 46 > data_.size() || rd_u32(data_, p) != kCentralSig) {
            error_ = "zip 中央目录损坏";
            err = error_;
            return false;
        }
        ZipEntryInfo e;
        e.method = rd_u16(data_, p + 10);
        e.crc = rd_u32(data_, p + 16);
        e.compressedSize = rd_u32(data_, p + 20);
        e.size = rd_u32(data_, p + 24);
        uint16_t nameLen = rd_u16(data_, p + 28);
        uint16_t extraLen = rd_u16(data_, p + 30);
        uint16_t commentLen = rd_u16(data_, p + 32);
        e.headerOffset = rd_u32(data_, p + 42);
        if (p + 46 + nameLen > data_.size()) {
            error_ = "zip 中央目录里的文件名越界";
            err = error_;
            return false;
        }
        e.name = normalize_entry_name(data_.substr(p + 46, nameLen));
        if (e.size == 0xFFFFFFFFu || e.compressedSize == 0xFFFFFFFFu ||
            e.headerOffset == 0xFFFFFFFFu) {
            apply_zip64_extra(data_, p + 46 + nameLen, extraLen, e);
        }
        entries_.push_back(e);
        p += 46 + static_cast<size_t>(nameLen) + extraLen + commentLen;
    }
    if (entries_.empty()) {
        error_ = "zip 里没有任何条目";
        err = error_;
        return false;
    }
    return true;
}

bool ZipReader::has(const std::string& name) const {
    for (const ZipEntryInfo& e : entries_) {
        if (e.name == name) return true;
    }
    return false;
}

std::string ZipReader::find_suffix(const std::string& suffix) const {
    for (const ZipEntryInfo& e : entries_) {
        if (e.name.size() >= suffix.size() &&
            e.name.compare(e.name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            return e.name;
        }
    }
    return std::string();
}

std::string ZipReader::find_basename(const std::string& base) const {
    for (const ZipEntryInfo& e : entries_) {
        size_t slash = e.name.find_last_of('/');
        std::string leaf = slash == std::string::npos ? e.name : e.name.substr(slash + 1);
        if (leaf == base) return e.name;
    }
    return std::string();
}

bool ZipReader::read(const std::string& name, std::string& out) {
    for (const ZipEntryInfo& e : entries_) {
        if (e.name != name) continue;
        return zip_read_entry(data_, e, out, error_);
    }
    error_ = str("zip 里没有 {}", name);
    return false;
}

}  // namespace poly

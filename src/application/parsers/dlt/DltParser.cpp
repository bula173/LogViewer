#include "DltParser.hpp"

#include "Error.hpp"
#include "LogEvent.hpp"
#include "Logger.hpp"
#include "Result.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace parser {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr uint8_t  kMagic[4]       = {'D', 'L', 'T', 0x01};
static constexpr size_t   kStorageHdrSize  = 16; // magic(4)+sec(4)+usec(4)+ecuid(4)
static constexpr size_t   kProgressBatch   = 512;

// Standard header flags (HTYP byte)
static constexpr uint8_t HTYP_UEH  = 0x01; // use extended header
static constexpr uint8_t HTYP_MSBF = 0x02; // payload is big-endian
static constexpr uint8_t HTYP_WEID = 0x04; // ECU ID follows length
static constexpr uint8_t HTYP_WSID = 0x08; // session ID follows ECU ID
static constexpr uint8_t HTYP_WTMS = 0x10; // timestamp follows session ID

// DLT message types (bits 1-3 of MSIN in extended header)
static constexpr uint8_t DLT_TYPE_LOG = 0;

// Verbose type-info bits
static constexpr uint32_t TI_TYLE = 0x0000000Fu;
static constexpr uint32_t TI_BOOL = (1u << 4);
static constexpr uint32_t TI_SINT = (1u << 5);
static constexpr uint32_t TI_UINT = (1u << 6);
static constexpr uint32_t TI_FLOA = (1u << 7);
static constexpr uint32_t TI_STRG = (1u << 9);
static constexpr uint32_t TI_RAWD = (1u << 10);
static constexpr uint32_t TI_VARI = (1u << 11);

static const char* const kLogLevels[] = {
    "Off", "Fatal", "Error", "Warn", "Info", "Debug", "Verbose"
};
static const char* const kMsgTypes[] = {
    "Log", "AppTrace", "NwTrace", "Control"
};

// ---------------------------------------------------------------------------
// Byte-order helpers
// ---------------------------------------------------------------------------

static uint16_t BE16(const uint8_t* p) noexcept
{
    return static_cast<uint16_t>((uint16_t{p[0]} << 8) | p[1]);
}
static uint32_t BE32(const uint8_t* p) noexcept
{
    return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16)
         | (uint32_t{p[2]} <<  8) |  uint32_t{p[3]};
}
static uint16_t LE16(const uint8_t* p) noexcept
{
    return static_cast<uint16_t>(uint16_t{p[0]} | (uint16_t{p[1]} << 8));
}
static uint32_t LE32(const uint8_t* p) noexcept
{
    return uint32_t{p[0]} | (uint32_t{p[1]} << 8)
         | (uint32_t{p[2]} << 16) | (uint32_t{p[3]} << 24);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Extract a null-padded 4-char ID (ECU/App/Ctx) as a trimmed std::string.
static std::string ReadId4(const uint8_t* p)
{
    int len = 4;
    while (len > 0 && p[len - 1] == '\0') --len;
    return {reinterpret_cast<const char*>(p), static_cast<size_t>(len)};
}

/// Hex dump of up to maxBytes bytes (space-separated uppercase).
static std::string HexDump(const uint8_t* data, size_t size, size_t maxBytes = 16)
{
    const size_t n = std::min(size, maxBytes);
    std::string s;
    s.reserve(n * 3);
    char buf[4];
    for (size_t i = 0; i < n; ++i)
    {
        if (i) s += ' ';
        std::snprintf(buf, sizeof(buf), "%02X", data[i]);
        s += buf;
    }
    if (size > maxBytes) s += " ...";
    return s;
}

// ---------------------------------------------------------------------------
// Verbose payload argument decoder
// ---------------------------------------------------------------------------

/// Decode one typed argument from `data[0..avail)`.
/// Sets `consumed` to the number of bytes consumed (0 on parse failure).
static std::string DecodeArg(const uint8_t* data, size_t avail,
                              size_t& consumed, bool msbf)
{
    consumed = 0;
    if (avail < 4) return {};

    const uint32_t ti = msbf ? BE32(data) : LE32(data);
    data += 4; avail -= 4; consumed = 4;

    // Skip optional variable-info name+unit strings.
    if (ti & TI_VARI)
    {
        if (avail < 4) { consumed = 0; return {}; }
        const uint16_t nameLen = msbf ? BE16(data) : LE16(data);
        const uint16_t unitLen = msbf ? BE16(data + 2) : LE16(data + 2);
        const size_t skip = 4u + nameLen + unitLen;
        if (avail < skip) { consumed = 0; return {}; }
        data += skip; avail -= skip; consumed += skip;
    }

    if (ti & TI_STRG)
    {
        if (avail < 2) { consumed = 0; return {}; }
        const uint16_t len = msbf ? BE16(data) : LE16(data);
        data += 2; avail -= 2; consumed += 2;
        if (len == 0 || avail < len) { consumed = 0; return {}; }
        const size_t slen = (data[len - 1] == '\0') ? len - 1u : len;
        consumed += len;
        return {reinterpret_cast<const char*>(data), slen};
    }

    if (ti & TI_BOOL)
    {
        if (avail < 1) { consumed = 0; return {}; }
        consumed += 1;
        return data[0] ? "true" : "false";
    }

    if (ti & (TI_SINT | TI_UINT | TI_FLOA))
    {
        const uint32_t tyle  = ti & TI_TYLE;
        const size_t   bytes = tyle ? (size_t{1} << (tyle - 1)) : 0;
        if (avail < bytes) { consumed = 0; return {}; }
        consumed += bytes;

        char buf[32];
        if (ti & TI_SINT)
        {
            int64_t v = 0;
            if      (bytes == 1) v = static_cast<int8_t>(data[0]);
            else if (bytes == 2) v = static_cast<int16_t>(msbf ? BE16(data) : LE16(data));
            else if (bytes == 4) v = static_cast<int32_t>(msbf ? BE32(data) : LE32(data));
            std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
        }
        else if (ti & TI_UINT)
        {
            uint64_t v = 0;
            if      (bytes == 1) v = data[0];
            else if (bytes == 2) v = msbf ? BE16(data) : LE16(data);
            else if (bytes == 4) v = msbf ? BE32(data) : LE32(data);
            std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(v));
        }
        else // TI_FLOA
        {
            if (bytes == 4)
            {
                float f; std::memcpy(&f, data, 4);
                std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(f));
            }
            else if (bytes == 8)
            {
                double d; std::memcpy(&d, data, 8);
                std::snprintf(buf, sizeof(buf), "%g", d);
            }
            else
            {
                std::snprintf(buf, sizeof(buf), "float?");
            }
        }
        return buf;
    }

    if (ti & TI_RAWD)
    {
        if (avail < 2) { consumed = 0; return {}; }
        const uint16_t len = msbf ? BE16(data) : LE16(data);
        consumed += 2 + len;
        char buf[24];
        std::snprintf(buf, sizeof(buf), "<raw %u bytes>", len);
        return buf;
    }

    return {}; // unsupported type
}

/// Decode all verbose arguments into a single space-joined string.
static std::string DecodeVerbose(const uint8_t* payload, size_t size,
                                 bool msbf, uint8_t numArgs)
{
    std::string result;
    size_t offset = 0;
    for (uint8_t a = 0; a < numArgs && offset < size; ++a)
    {
        size_t consumed = 0;
        std::string part = DecodeArg(payload + offset, size - offset, consumed, msbf);
        if (consumed == 0) break;
        offset += consumed;
        if (part.empty()) continue;
        if (!result.empty()) result += ' ';
        result += part;
    }
    return result;
}

// ---------------------------------------------------------------------------
// ParseData entry points
// ---------------------------------------------------------------------------

void DltParser::ParseData(const std::filesystem::path& filepath)
{
    util::Logger::Debug("[DltParser] opening file: {}", filepath.string());

    std::ifstream file(filepath, std::ios::binary);
    if (!file)
    {
        util::Logger::Error("[DltParser] cannot open file: {}", filepath.string());
        throw std::runtime_error("DltParser: cannot open: " + filepath.string());
    }

    std::error_code ec;
    const auto size = std::filesystem::file_size(filepath, ec);
    m_totalProgress   = (!ec && size <= UINT32_MAX) ? static_cast<uint32_t>(size) : 0;
    m_currentProgress = 0;
    m_eventId         = 0;

    auto result = ParseStream(file);
    if (result.isErr())
        util::Logger::Warn("[DltParser] ParseStream returned error: {}", result.error().what());
}

void DltParser::ParseData(std::istream& input)
{
    util::Logger::Debug("[DltParser] parsing from stream");
    m_totalProgress   = 0;
    m_currentProgress = 0;
    m_eventId         = 0;
    auto result = ParseStream(input);
    if (result.isErr())
        util::Logger::Warn("[DltParser] ParseStream returned error: {}", result.error().what());
}

// ---------------------------------------------------------------------------
// Core streaming loop
// ---------------------------------------------------------------------------

util::Result<int, error::Error> DltParser::ParseStream(std::istream& input)
{
    using EventItems = db::LogEvent::EventItems;
    using R = util::Result<int, error::Error>;

    std::vector<std::pair<int, EventItems>> batch;
    batch.reserve(kProgressBatch);

    // ── Detect whether file uses storage headers ──────────────────────────
    uint8_t probe[4] = {};
    input.read(reinterpret_cast<char*>(probe), 4);
    if (input.gcount() < 4)
    {
        util::Logger::Debug("[DltParser] stream too short to probe magic — no messages");
        return R::Ok(0);
    }

    const bool hasStorage = (std::memcmp(probe, kMagic, 4) == 0);
    util::Logger::Debug("[DltParser] storage headers: {}", hasStorage ? "present" : "absent");
    input.seekg(0, std::ios::beg); // rewind; the loop handles the header

    // ── Reusable body buffer ─────────────────────────────────────────────
    std::vector<uint8_t> body;

    // ── Message loop ─────────────────────────────────────────────────────
    while (input.good())
    {
        double   storageSec = 0.0;
        uint32_t storageUsec = 0;
        std::string storageEcu;

        // --- Storage header ---
        if (hasStorage)
        {
            uint8_t sh[kStorageHdrSize];
            input.read(reinterpret_cast<char*>(sh), kStorageHdrSize);
            const auto got = static_cast<size_t>(input.gcount());
            m_currentProgress += static_cast<uint32_t>(got);
            if (got < kStorageHdrSize) break;
            if (std::memcmp(sh, kMagic, 4) != 0) break; // lost sync

            storageSec  = static_cast<double>(LE32(sh + 4));
            storageUsec = LE32(sh + 8);
            storageEcu  = ReadId4(sh + 12);
        }

        // --- Standard header (always 4 bytes: htyp, ctr, len_BE16) ---
        uint8_t stdh[4];
        input.read(reinterpret_cast<char*>(stdh), 4);
        const auto gotStd = static_cast<size_t>(input.gcount());
        m_currentProgress += static_cast<uint32_t>(gotStd);
        if (gotStd < 4) break;

        const uint8_t  htyp   = stdh[0];
        const uint8_t  msgCtr = stdh[1];
        const uint16_t length = BE16(stdh + 2); // total standard message size

        // Length must at least cover the 4 bytes we already consumed.
        if (length < 4)
        {
            util::Logger::Warn("[DltParser] malformed length {}, skipping", length);
            continue;
        }

        const size_t bodySize = length - 4;
        body.resize(bodySize);
        if (bodySize > 0)
        {
            input.read(reinterpret_cast<char*>(body.data()),
                       static_cast<std::streamsize>(bodySize));
            const auto gotBody = static_cast<size_t>(input.gcount());
            m_currentProgress += static_cast<uint32_t>(gotBody);
            if (gotBody < bodySize) break;
        }

        size_t pos = 0; // cursor into body[]

        // --- Optional standard-header fields ---
        std::string ecuId = storageEcu;
        uint32_t sessionId = 0;
        double timestamp = storageSec + static_cast<double>(storageUsec) / 1e6;

        if ((htyp & HTYP_WEID) && pos + 4 <= bodySize)
        {
            ecuId = ReadId4(body.data() + pos);
            pos  += 4;
        }
        if ((htyp & HTYP_WSID) && pos + 4 <= bodySize)
        {
            sessionId = BE32(body.data() + pos);
            pos      += 4;
        }
        if ((htyp & HTYP_WTMS) && pos + 4 <= bodySize)
        {
            // Timestamp is in 0.1 ms units.
            timestamp = static_cast<double>(BE32(body.data() + pos)) / 10000.0;
            pos      += 4;
        }

        const bool msbf = (htyp & HTYP_MSBF) != 0;

        // --- Extended header (10 bytes) ---
        std::string appId, ctxId;
        std::string level   = "Info";
        std::string msgType = "Log";
        bool    isVerbose = false;
        uint8_t numArgs   = 0;

        if ((htyp & HTYP_UEH) && pos + 10 <= bodySize)
        {
            const uint8_t msin = body[pos];
            isVerbose = (msin & 0x01) != 0;
            const uint8_t type   = (msin >> 1) & 0x07;
            const uint8_t logLvl = (msin >> 4) & 0x0F;
            numArgs = body[pos + 1];
            appId   = ReadId4(body.data() + pos + 2);
            ctxId   = ReadId4(body.data() + pos + 6);
            pos    += 10;

            if (type < 4)
            {
                msgType = kMsgTypes[type];
            }
            else
            {
                util::Logger::Warn("[DltParser] unknown message type {} in msg #{}, "
                    "app={} ctx={}", type, m_eventId, appId, ctxId);
            }

            if (type == DLT_TYPE_LOG && logLvl < 7)
                level = kLogLevels[logLvl];

            util::Logger::Trace("[DltParser] msg #{}: app={} ctx={} type={} level={} "
                "verbose={} args={}", m_eventId, appId, ctxId, msgType, level,
                isVerbose, numArgs);
        }
        else
        {
            util::Logger::Trace("[DltParser] msg #{}: no extended header, ctr={}", m_eventId, msgCtr);
        }

        // --- Payload ---
        const size_t payloadSize = (pos <= bodySize) ? (bodySize - pos) : 0;
        std::string info;

        if (payloadSize > 0)
        {
            const uint8_t* payload = body.data() + pos;
            if (isVerbose && numArgs > 0)
            {
                info = DecodeVerbose(payload, payloadSize, msbf, numArgs);
            }
            else if (!isVerbose && payloadSize >= 4)
            {
                // Non-verbose: 4-byte message ID + optional raw data.
                const uint32_t msgId = msbf ? BE32(payload) : LE32(payload);
                char buf[16];
                std::snprintf(buf, sizeof(buf), "0x%08X", msgId);
                info = std::string("MsgID=") + buf;
                if (payloadSize > 4)
                    info += "  " + HexDump(payload + 4, payloadSize - 4);
            }
            else
            {
                info = HexDump(payload, payloadSize);
            }
        }

        // --- Build event ---
        EventItems items;
        items.reserve(8);

        char tsBuf[32];
        std::snprintf(tsBuf, sizeof(tsBuf), "%.6f", timestamp);
        items.emplace_back("timestamp", tsBuf);
        items.emplace_back("level",     level);
        items.emplace_back("type",      msgType);
        items.emplace_back("AppID",     std::move(appId));
        items.emplace_back("ContextID", std::move(ctxId));
        items.emplace_back("EcuID",     std::move(ecuId));
        if (!info.empty())
            items.emplace_back("info", std::move(info));

        char ctrBuf[8];
        std::snprintf(ctrBuf, sizeof(ctrBuf), "%u", msgCtr);
        items.emplace_back("MsgCtr", ctrBuf);

        if (sessionId != 0)
        {
            char sidBuf[12];
            std::snprintf(sidBuf, sizeof(sidBuf), "%u", sessionId);
            items.emplace_back("SessionID", sidBuf);
        }

        batch.emplace_back(m_eventId++, std::move(items));

        if (batch.size() >= kProgressBatch)
        {
            NotifyNewEventBatch(std::move(batch));
            NotifyProgressUpdated();
            batch.clear();
            batch.reserve(kProgressBatch);
        }
    }

    if (!batch.empty())
    {
        NotifyNewEventBatch(std::move(batch));
        NotifyProgressUpdated();
    }

    util::Logger::Info("[DltParser] parse complete: {} message(s) emitted", m_eventId);
    return R::Ok(m_eventId);
}

} // namespace parser

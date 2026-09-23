#ifndef ADV_SCANNER_HPP
#define ADV_SCANNER_HPP

#include "context.hpp"
#include "options.hpp"
#include "parser.hpp"

#if !defined(yyFlexLexerOnce)
#include <FlexLexer.h>
#endif

namespace adv {

class Scanner : public yyFlexLexer {
public:
    Scanner(
        ContextBase& ctx,
        const Options& options,
        std::span<const adv_source_t> sources,
        const std::filesystem::path& file,
        const std::filesystem::path& basePath,
        adv_uint32_t depth)
        : yyFlexLexer(), _ctx(ctx), _options(options) {
        import(file, sources, basePath, depth);
    }

    ~Scanner() {
        if (_needRelease) {
            adv_data_t data{};
            if (auto releaseImport = _options.getReleaseImport(&data)) {
                releaseImport(const_cast<adv_source_t*>(_source), data);
            }
        }
        if (_buffer) {
            yy_delete_buffer(_buffer);
            _buffer = nullptr;
        }
    }

    int yylex(Parser::semantic_type* yylval, Parser::location_type* yylloc);

    [[nodiscard]] ContextBase& context() noexcept { return _ctx; }

    [[nodiscard]] const std::string* filename() const noexcept { return _filename.empty() ? nullptr : &_filename; }

    template <adv_status_t Status, typename... Args>
    void log(const adv_location_t& loc, Args&&... args) {
        _ctx.log<Status>(_options, loc, std::forward<Args>(args)...);
    }

    void action(adv::location& loc) {
        loc.step();
        loc.columns(yyleng);
    }

private:
    class MemoryBuffer : public std::streambuf {
    public:
        MemoryBuffer(const char* data, size_t size) {
            setg(const_cast<char*>(data), const_cast<char*>(data), const_cast<char*>(data) + size);
        }
    };

    void import(
        const std::filesystem::path& file,
        std::span<const adv_source_t> sources,
        const std::filesystem::path& basePath,
        adv_uint32_t depth) {
        const auto [path, source, needRelease] = findFile(file, sources, basePath, depth, _options);
        if (path.empty()) {
            // _ctx.log<IDL_STATUS_E3021>(astLoc, file.filename().string());
            return;
        }
        _source      = source;
        _needRelease = needRelease;

        if (source) {
            _memBuffer = std::make_unique<MemoryBuffer>(source->data, (size_t)source->size);
            _stream    = std::make_unique<std::istream>(_memBuffer.get());
        } else {
            _stream = std::make_unique<std::ifstream>(path);
            if (_stream->fail()) {
                _stream.reset();
                // _ctx.log<IDL_STATUS_E3022>(astLoc, file.string());
                return;
            }
        }
        _buffer = yy_create_buffer(*_stream.get(), 16384);
        yy_switch_to_buffer(_buffer);
        yylineno = 1;

        _ctx.addImport(path);
        _filename = path.string();
    }

    static std::filesystem::path normalize(const std::filesystem::path& path, const std::filesystem::path& basePath) {
        auto filename = path.is_absolute() ? std::filesystem::relative(path, basePath) : path;
        filename.make_preferred();
        return filename;
    }

    static std::tuple<std::filesystem::path, const adv_source_t*, bool> findFile(
        const std::filesystem::path& file,
        std::span<const adv_source_t> sources,
        const std::filesystem::path& basePath,
        adv_uint32_t depth,
        const Options& options) {
        if (file.empty() && !sources.empty()) {
            auto& source = sources.front();
            return {source.name, &source, false};
        }

        auto filename = normalize(file, basePath);
        auto name     = filename.string();

        adv_data_t data{};
        if (auto importer = options.getImporter(&data)) {
            if (auto source = importer(name.c_str(), depth, data)) {
                return {filename, source, true};
            }
        }

        for (const auto& source : sources) {
            auto sourceName = normalize(source.name, basePath);
            if (filename == sourceName) {
                return {name, &source, false};
            }
        }

        if (file.is_absolute()) {
            if (!std::filesystem::exists(file) || !std::filesystem::is_regular_file(file)) {
                return {{}, nullptr, false};
            }
            return {file, nullptr, false};
        }

        std::vector<std::filesystem::path> importDirs{};
        importDirs.push_back(basePath);
        adv_uint32_t dirCount{};
        options.getImportDirs(dirCount, nullptr);
        std::vector<adv_utf8_t> dirs;
        dirs.resize(dirCount);
        options.getImportDirs(dirCount, dirs.data());
        importDirs.reserve(dirs.size() + 1);
        for (const auto dir : dirs) {
            importDirs.push_back(dir);
        }

        for (const auto base : importDirs) {
            auto fullpath = base / file;
            fullpath.make_preferred();
            if (std::filesystem::exists(fullpath) && std::filesystem::is_regular_file(fullpath)) {
                return {fullpath, nullptr, false};
            }
        }
        return {{}, nullptr, false};
    }

    [[nodiscard]] adv_float32_t parseFloat(const adv_location_t& location, std::string_view str) noexcept {
        adv_float32_t val  = 0.0f;
        const auto [_, ec] = std::from_chars(str.data(), str.data() + str.length(), val);
        if (ec == std::errc()) {
            return val;
        } else if (ec == std::errc::result_out_of_range) {
            constexpr auto minVal = std::numeric_limits<adv_float32_t>::lowest();
            constexpr auto maxVal = std::numeric_limits<adv_float32_t>::max();
            log<ADV_STATUS_W2001>(location, "Float-point", str, minVal, maxVal);
        } else {
            log<ADV_STATUS_E3002>(location);
        }
        return 0.0f;
    }

    [[nodiscard]] adv_sint32_t parseInt(const adv_location_t& location, std::string_view str) noexcept {
        adv_sint32_t val   = 0;
        const auto [_, ec] = std::from_chars(str.data(), str.data() + str.length(), val);
        if (ec == std::errc()) {
            return val;
        } else if (ec == std::errc::result_out_of_range) {
            constexpr auto minVal = std::numeric_limits<adv_sint32_t>::min();
            constexpr auto maxVal = std::numeric_limits<adv_sint32_t>::max();
            log<ADV_STATUS_W2001>(location, "Integer", str, minVal, maxVal);
        } else {
            log<ADV_STATUS_E3002>(location);
        }
        return 0;
    }

    [[nodiscard]] std::string unescape(const adv_location_t& location, const std::string_view str) {
        auto cStr = str.data();
        std::ostringstream ss;
        for (size_t i = 0; i < str.length(); ++i) {
            char c  = *cStr++;
            char nc = *cStr;
            if (c == '\\') {
                if (i + 1 < str.length()) {
                    log<ADV_STATUS_E3005>(location, "incomplete sequence of '\\'");
                    break;
                } else if (nc == '\\') {
                    ss << '\\';
                } else if (nc == '"') {
                    ss << '"';
                } else if (nc == 'r') {
                    ss << '\r';
                } else if (nc == 'n') {
                    ss << '\n';
                } else if (nc == 't') {
                    ss << '\t';
                } else {
                    log<ADV_STATUS_E3005>(location, "valid sequences: '\\\\', '\\\"', '\\r', '\\n', '\\t'");
                    break;
                }
                ++cStr;
            } else {
                ss << c;
            }
        }
        return ss.str();
    }

    ContextBase& _ctx;
    const Options& _options;
    std::string _filename{};
    const adv_source_t* _source{};
    bool _needRelease{};
    yy_buffer_state* _buffer{};
    std::unique_ptr<MemoryBuffer> _memBuffer{};
    std::unique_ptr<std::istream> _stream{};

    // std::span<const adv_source_t> _sources;
    // std::filesystem::path _basePath{};
};

} // namespace adv

#endif // ADV_SCANNER_HPP

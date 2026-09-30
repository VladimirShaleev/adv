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
        Context& ctx,
        const Options& options,
        std::span<const adv_source_t> sources,
        const adv_location_t& loc,
        const std::filesystem::path& file,
        const std::filesystem::path& basePath,
        adv_uint32_t depth)
        : yyFlexLexer(), _ctx(ctx), _options(options) {
        import(loc, file, sources, basePath, depth);
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

    [[nodiscard]] Context& context() noexcept { return _ctx; }

    [[nodiscard]] const std::string* filename() const noexcept { return _filename.empty() ? nullptr : &_filename; }

    template <adv_status_t Status, typename... Args>
    void log(const adv_location_t& loc, Args&&... args) {
        _ctx.log<Status>(_options, loc, std::forward<Args>(args)...);
    }

    void action(adv::location& loc) {
        loc.step();
        loc.columns(yyleng);
    }

    [[nodiscard]] bool parseAdvt() {
        assert(filename());
        auto size = _stream->tellg();
        _stream->seekg(0, std::ios::end);
        size = _stream->tellg() - size;
        _stream->seekg(0, std::ios::beg);

        std::vector<char> data;
        data.reserve(size);
        std::copy(
            std::istreambuf_iterator<char>(*_stream.get()), std::istreambuf_iterator<char>(), std::back_inserter(data));

        utils::TranslateResult err{};
        auto bytecode = std::move(utils::translateTextToBinary({data.data(), data.size()}, err));
        if (err.format != utils::Format::Advt) {
            return false;
        }
        _ctx.clearMessages();
        if (err.errorCode != utils::TranslateResult::NoError) {
            std::string_view fname{filename()->c_str(), filename()->length()};
            adv_location_t loc{_ctx.intern(fname), err.line, err.column};
            if (err.message.empty()) {
                _ctx.log<ADV_STATUS_E3042>(_options, loc);
            } else {
                _ctx.log<ADV_STATUS_E3042>(_options, loc, err.message);
            }
            return false;
        }
        _ctx.setBytecode(std::move(bytecode), err.format);
        return true;
    }

private:
    class MemoryBuffer : public std::streambuf {
    public:
        MemoryBuffer(const char* data, size_t size) {
            setg(const_cast<char*>(data), const_cast<char*>(data), const_cast<char*>(data) + size);
        }
    };

    void import(
        const adv_location_t& loc,
        const std::filesystem::path& file,
        std::span<const adv_source_t> sources,
        const std::filesystem::path& basePath,
        adv_uint32_t depth) {
        const auto [path, source, needRelease] = findFile(file, sources, basePath, depth, _options);
        if (path.empty()) {
            _ctx.log<ADV_STATUS_E3037>(_options, loc, file.filename().string());
            return;
        }
        _source = source;
        _needRelease = needRelease;

        if (depth == 0 && readBytecode(source, path)) {
            std::span bytecode{_ctx.bytecode().data(), _ctx.bytecode().size()};
            vm_uint32_t sum = 0;
            auto block0 = bytecode.subspan(0, offsetof(vm_script_header_t, checksum));
            auto block1 = std::span{(const adv_uint8_t*)&sum, sizeof(vm_uint32_t)};
            auto block2 = bytecode.subspan(sizeof(vm_script_header_t));
            auto view = std::array<std::span<const uint8_t>, 3>{block0, block1, block2} | std::views::join;
            const auto result = utils::adler32(std::move(view));

            auto checksum = reinterpret_cast<const vm_script_header_t*>(bytecode.data())->checksum;
            auto version = reinterpret_cast<const vm_script_header_t*>(bytecode.data())->version;
            if (std::endian::native != std::endian::little) {
                checksum = utils::swapBytes(checksum);
                version = utils::swapBytes(version);
            }
            if (checksum != result) {
                _ctx.setBytecode({}, utils::Format::Unknown);
                _ctx.log<ADV_STATUS_E3043>(_options, loc, result, checksum);
            }
            if ((version >> 16) == (ADV_VERSION >> 16)) {
                _ctx.setBytecode({}, utils::Format::Unknown);
                auto verStr = fmt::format("{}.{}.{}", (version >> 16) & 0xFF, (version >> 8) & 0xFF, version & 0xFF);
                _ctx.log<ADV_STATUS_E3044>(_options, loc, verStr, ADV_VERSION_STRING);
            }
            return;
        }

        if (source) {
            _memBuffer = std::make_unique<MemoryBuffer>(source->data, (size_t)source->size);
            _stream = std::make_unique<std::istream>(_memBuffer.get());
        } else {
            _stream = std::make_unique<std::ifstream>(path);
            if (_stream->fail()) {
                _stream.reset();
                _ctx.log<ADV_STATUS_E3038>(_options, loc, file.string());
                return;
            }
        }
        _buffer = yy_create_buffer(*_stream.get(), 16384);
        yy_switch_to_buffer(_buffer);
        yylineno = 1;

        _ctx.addImport(path);
        _filename = path.string();
    }

    [[nodiscard]] static std::filesystem::path
    normalize(const std::filesystem::path& path, const std::filesystem::path& basePath) {
        auto filename = path.is_absolute() ? std::filesystem::relative(path, basePath) : path;
        filename.make_preferred();
        return filename;
    }

    [[nodiscard]] static std::tuple<std::filesystem::path, const adv_source_t*, bool> findFile(
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
        auto name = filename.string();

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

    [[nodiscard]] bool readBytecode(const adv_source_t* source, const std::filesystem::path& filename) noexcept {
        if (source) {
            if (utils::isBinaryCode({source->data, source->size})) {
                std::vector<adv_uint8_t> bytecode(
                    (adv_uint8_t*)source->data, (adv_uint8_t*)source->data + source->size);
                _ctx.setBytecode(std::move(bytecode), utils::Format::Advb);
                return true;
            }
        }
        std::ifstream stream(filename, std::ios::binary);
        if (!stream.is_open()) {
            return false;
        }
        std::array<adv_char_t, sizeof(vm_script_header_t) + sizeof(vm_script_table_t)> bytes;
        if (!stream.read(bytes.data(), bytes.size())) {
            stream.close();
            return false;
        }
        if (utils::isBinaryCode({bytes.data(), bytes.size()}, false)) {
            stream.seekg(0, std::ios::beg);
            auto size = stream.tellg();
            stream.seekg(0, std::ios::end);
            size = stream.tellg() - size;
            stream.seekg(0, std::ios::beg);

            std::vector<adv_uint8_t> bytecode;
            bytecode.resize(size);
            if (stream.read((char*)bytecode.data(), size) &&
                utils::isBinaryCode({(const char*)bytecode.data(), bytecode.size()})) {
                stream.close();
                _ctx.setBytecode(std::move(bytecode), utils::Format::Advb);
                return true;
            }
        }
        stream.close();
        return false;
    }

    [[nodiscard]] adv_float32_t parseFloat(const adv_location_t& location, std::string_view str) noexcept {
        adv_float32_t val = 0.0f;
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
        adv_sint32_t val = 0;
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
        if (auto result = utils::unescape(str)) {
            return result.value();
        }
        log<ADV_STATUS_E3005>(location, "valid sequences: '\\\\', '\\\"', '\\r', '\\n', '\\t'");
        return "";
    }

    Context& _ctx;
    const Options& _options;
    std::string _filename{};
    const adv_source_t* _source{};
    bool _needRelease{};
    yy_buffer_state* _buffer{};
    std::unique_ptr<MemoryBuffer> _memBuffer{};
    std::unique_ptr<std::istream> _stream{};
};

} // namespace adv

#endif // ADV_SCANNER_HPP

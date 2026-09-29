#ifndef ADV_OPTIONS_HPP
#define ADV_OPTIONS_HPP

#include "compiler.hpp"

struct _adv_options : public utils::Object {};

namespace adv {

class Options final : public _adv_options {
public:
    [[nodiscard]] bool getWarningsAsErrors() const noexcept;
    void setWarningsAsErrors(bool enable) noexcept;

    [[nodiscard]] adv_format_t getOutputFormat() const noexcept;
    void setOutputFormat(adv_format_t format) noexcept;

    [[nodiscard]] adv_utf8_t getOutputDir() const noexcept;
    void setOutputDir(adv_utf8_t dir);

    void getImportDirs(adv_uint32_t& dirCount, adv_utf8_t* dirs) const noexcept;
    void setImportDirs(std::span<const adv_utf8_t> dirs);

    [[nodiscard]] adv_import_callback_t getImporter(adv_data_t* data) const noexcept;
    void setImporter(adv_import_callback_t callback, adv_data_t data) noexcept;

    [[nodiscard]] adv_release_import_callback_t getReleaseImport(adv_data_t* data) const noexcept;
    void setReleaseImport(adv_release_import_callback_t callback, adv_data_t data) noexcept;

    [[nodiscard]] adv_write_callback_t getWriter(adv_data_t* data) const noexcept;
    void setWriter(adv_write_callback_t callback, adv_data_t data) noexcept;

private:
    bool _warningsAsErrors{};

    adv_format_t _outputFormat{};
    std::string _outputDir{};
    std::vector<std::string> _importDirs{};

    adv_import_callback_t _importer{};
    adv_release_import_callback_t _releaseImport{};
    adv_write_callback_t _writer{};

    adv_data_t _importerData{};
    adv_data_t _releaseImportData{};
    adv_data_t _writerData{};
};

} // namespace adv

#endif // ADV_OPTIONS_HPP

#include "options.hpp"

namespace adv {

bool Options::getWarningsAsErrors() const noexcept { return _warningsAsErrors; }

void Options::setWarningsAsErrors(bool enable) noexcept { _warningsAsErrors = enable; }

adv_utf8_t Options::getOutputDir() const noexcept { return _outputDir.c_str(); }

void Options::setOutputDir(adv_utf8_t dir) { _outputDir = dir; }

void Options::getImportDirs(adv_uint32_t& dirCount, adv_utf8_t* dirs) const noexcept {
    if (dirs) {
        dirCount = std::min(dirCount, (adv_uint32_t)_importDirs.size());
        for (size_t i = 0; i < dirCount; ++i) {
            dirs[i] = _importDirs[i].c_str();
        }
    } else {
        dirCount = (adv_uint32_t)_importDirs.size();
    }
}

void Options::setImportDirs(std::span<const adv_utf8_t> dirs) {
    _importDirs.clear();
    _importDirs.reserve(dirs.size());
    std::copy(dirs.begin(), dirs.end(), std::back_inserter(_importDirs));
}

adv_import_callback_t Options::getImporter(adv_data_t* data) const noexcept {
    if (data) {
        *data = _importerData;
    }
    return _importer;
}

void Options::setImporter(adv_import_callback_t callback, adv_data_t data) noexcept {
    if (callback) {
        _importer     = callback;
        _importerData = data;
    } else {
        _importer     = nullptr;
        _importerData = nullptr;
    }
}

adv_release_import_callback_t Options::getReleaseImport(adv_data_t* data) const noexcept {
    if (data) {
        *data = _releaseImportData;
    }
    return _releaseImport;
}

void Options::setReleaseImport(adv_release_import_callback_t callback, adv_data_t data) noexcept {
    if (callback) {
        _releaseImport     = callback;
        _releaseImportData = data;
    } else {
        _releaseImport     = nullptr;
        _releaseImportData = nullptr;
    }
}

adv_write_callback_t Options::getWriter(adv_data_t* data) const noexcept {
    if (data) {
        *data = _writerData;
    }
    return _writer;
}

void Options::setWriter(adv_write_callback_t callback, adv_data_t data) noexcept {
    if (callback) {
        _writer     = callback;
        _writerData = data;
    } else {
        _writer     = nullptr;
        _writerData = nullptr;
    }
}

} // namespace adv

adv_result_t adv_options_create(adv_options_t* options) {
    return options ? utils::Object::create<adv_result_t, adv::Options>(*options) : ADV_RESULT_ERROR_INVALID_ARG;
}

adv_options_t adv_options_reference(adv_options_t options) {
    if (options) {
        options->as<adv::Options>()->reference();
    }
    return options;
}

void adv_options_destroy(adv_options_t options) {
    if (options) {
        options->as<adv::Options>()->destroy();
    }
}

adv_bool_t adv_options_get_warnings_as_errors(adv_options_t options) {
    return options ? options->as<adv::Options>()->getWarningsAsErrors() : 0;
}

void adv_options_set_warnings_as_errors(adv_options_t options, adv_bool_t enable) {
    if (options) {
        options->as<adv::Options>()->setWarningsAsErrors(!!enable);
    }
}

adv_utf8_t adv_options_get_output_dir(adv_options_t options) {
    return options ? options->as<adv::Options>()->getOutputDir() : nullptr;
}

void adv_options_set_output_dir(adv_options_t options, adv_utf8_t dir) {
    if (options) {
        options->as<adv::Options>()->setOutputDir(dir);
    }
}

void adv_options_get_import_dirs(adv_options_t options, adv_uint32_t* dir_count, adv_utf8_t* dirs) {
    if (dir_count) {
        if (options) {
            options->as<adv::Options>()->getImportDirs(*dir_count, dirs);
        } else {
            *dir_count = 0;
        }
    }
}

void adv_options_set_import_dirs(adv_options_t options, adv_uint32_t dir_count, const adv_utf8_t* dirs) {
    if (options) {
        options->as<adv::Options>()->setImportDirs({dirs, dir_count});
    }
}

adv_import_callback_t adv_options_get_importer(adv_options_t options, adv_data_t* data) {
    return options ? options->as<adv::Options>()->getImporter(data) : nullptr;
}

void adv_options_set_importer(adv_options_t options, adv_import_callback_t callback, adv_data_t data) {
    if (options) {
        options->as<adv::Options>()->setImporter(callback, data);
    }
}

adv_release_import_callback_t adv_options_get_release_import(adv_options_t options, adv_data_t* data) {
    return options ? options->as<adv::Options>()->getReleaseImport(data) : nullptr;
}

void adv_options_set_release_import(adv_options_t options, adv_release_import_callback_t callback, adv_data_t data) {
    if (options) {
        options->as<adv::Options>()->setReleaseImport(callback, data);
    }
}

adv_write_callback_t adv_options_get_writer(adv_options_t options, adv_data_t* data) {
    return options ? options->as<adv::Options>()->getWriter(data) : nullptr;
}

void adv_options_set_writer(adv_options_t options, adv_write_callback_t callback, adv_data_t data) {
    if (options) {
        options->as<adv::Options>()->setWriter(callback, data);
    }
}

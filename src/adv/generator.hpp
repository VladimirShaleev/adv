#ifndef ADV_GENERATOR_HPP
#define ADV_GENERATOR_HPP

namespace adv {

class Options;
class Context;

class Generator {
public:
    Generator(const Options& options, Context& ctx) noexcept;

    [[nodiscard]] adv_result_t gen();
    [[nodiscard]] adv_result_t text();

private:
    void saveBinary(
        const std::vector<uint8_t>& exportTable, const std::vector<uint8_t>& data, const std::vector<uint8_t>& code);

    const Options& _options;
    Context& _ctx;
};

} // namespace adv

#endif // ADV_GENERATOR_HPP

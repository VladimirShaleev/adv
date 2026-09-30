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
    void saveBinary();
    
    const Options& _options;
    Context& _ctx;
    std::vector<uint8_t> _data;
    std::vector<uint8_t> _code;
};

} // namespace adv

#endif // ADV_GENERATOR_HPP

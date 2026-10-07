#include <benchmark/benchmark.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstdint>
#include <vector>

namespace {

struct JpegBuffer {
    std::vector<std::uint8_t> bytes;
};

void WriteJpeg(void* context, void* data, int size) {
    auto* out = static_cast<JpegBuffer*>(context);
    auto* begin = static_cast<std::uint8_t*>(data);
    out->bytes.insert(out->bytes.end(), begin, begin + size);
}

void BM_JpegEncode1232x832Q85(benchmark::State& state) {
    constexpr int kWidth = 1232;
    constexpr int kHeight = 832;
    constexpr int kChannels = 3;
    constexpr int kQuality = 85;

    std::vector<std::uint8_t> rgb(
        static_cast<size_t>(kWidth) * kHeight * kChannels,
        127
    );

    JpegBuffer output;
    output.bytes.reserve(static_cast<size_t>(kWidth) * kHeight);

    for (auto _ : state) {
        output.bytes.clear();
        const int ok = stbi_write_jpg_to_func(
            WriteJpeg,
            &output,
            kWidth,
            kHeight,
            kChannels,
            rgb.data(),
            kQuality
        );
        benchmark::DoNotOptimize(ok);
        benchmark::DoNotOptimize(output.bytes.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_JpegEncode1232x832Q85);

} // namespace

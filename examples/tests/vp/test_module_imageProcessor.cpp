#include "module/vp/module_imageProcessor.hpp"
#include "module/module_app.hpp"
#include "module/vo/module_rendererVideo.hpp"
#include "tests/module_test_utils.hpp"

#ifdef None
#undef None
#endif

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include <unistd.h>

using namespace FFMedia;
using FFMediaTest::CliOptions;
using FFMediaTest::RunMonitor;

namespace
{

struct ImageOptions {
    std::string format = "RGB24";
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t hstride = 0;
    uint32_t vstride = 0;
    bool afbc = false;
};

struct CoverOptions {
    ImageCrop rect = {};
    uint32_t color = 0xff000000U;
};

enum class BlendAlphaMode : int32_t {
    Opaque = 0,
    Straight = 1,
    Premultiplied = 2,
};

struct BlendOptions {
    ImageCrop rect = {};
    uint32_t color = 0xffffffffU;
    float opacity = 1.0f;
    BlendAlphaMode alpha_mode = BlendAlphaMode::Straight;
};

struct BlendInputBuffer {
    MediaChannelId input_id = 0;
    std::shared_ptr<VideoBuffer> buffer;
};

enum class DisplayMode {
    None,
    Input,
    Output,
    Both,
};

struct Options {
    ImageOptions input;
    ImageOptions output;
    std::string input_file;
    std::string output_file;
    bool output_format_set = false;
    bool output_width_set = false;
    bool output_height_set = false;
    bool output_afbc_set = false;
    bool crop_set = false;
    ImageCrop crop = {};
    int32_t rotation = 0;
    bool mirror = false;
    bool flip = false;
    std::vector<CoverOptions> covers;
    std::vector<BlendOptions> blends;
    bool change_output = false;
    uint32_t warmup = 20;
    uint32_t iterations = 300;
    DisplayMode display = DisplayMode::None;
    uint32_t display_duration = 10;
    uint32_t display_width = 640;
    uint32_t display_height = 360;
};

struct BenchmarkResult {
    double total_ms = 0.0;
    double fps = 0.0;
    double output_mpixels = 0.0;
    double estimated_gib_per_second = 0.0;
    std::vector<double> latency_ms;
    std::shared_ptr<VideoBuffer> last_output;
};

std::string uppercase(std::string name)
{
    std::string normalized;
    normalized.reserve(name.size());
    for (char character : name) {
        normalized.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(character))));
    }
    return normalized;
}

bool parseDisplayMode(const std::string& text, DisplayMode& mode)
{
    const std::string value = uppercase(text);
    if (value == "NONE" || value == "OFF" || value == "0")
        mode = DisplayMode::None;
    else if (value == "INPUT")
        mode = DisplayMode::Input;
    else if (value == "OUTPUT")
        mode = DisplayMode::Output;
    else if (value == "BOTH")
        mode = DisplayMode::Both;
    else
        return false;
    return true;
}

const char* displayModeName(DisplayMode mode)
{
    switch (mode) {
        case DisplayMode::Input:
            return "input";
        case DisplayMode::Output:
            return "output";
        case DisplayMode::Both:
            return "both";
        default:
            return "none";
    }
}

bool parseUnsigned(const std::string& text, uint32_t minimum,
                   uint32_t maximum, uint32_t& value)
{
    if (text.empty() || text[0] == '-')
        return false;
    char* end = nullptr;
    errno = 0;
    const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
    if (errno != 0 || !end || *end != '\0' || parsed < minimum
        || parsed > maximum) {
        return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

bool parseSize(const std::string& text, uint32_t& width, uint32_t& height)
{
    const size_t separator = text.find_first_of("xX");
    if (separator == std::string::npos || separator == 0
        || separator + 1 >= text.size()) {
        return false;
    }
    return parseUnsigned(text.substr(0, separator), 1, 16384, width)
           && parseUnsigned(text.substr(separator + 1), 1, 16384, height);
}

bool parseAfbc(const std::string& text, bool& afbc)
{
    const std::string value = uppercase(text);
    if (value == "1" || value == "ON" || value == "TRUE"
        || value == "AFBC") {
        afbc = true;
        return true;
    }
    if (value == "0" || value == "OFF" || value == "FALSE"
        || value == "LINEAR") {
        afbc = false;
        return true;
    }
    return false;
}

bool parseBoolean(const std::string& text, bool& value)
{
    const std::string normalized = uppercase(text);
    if (normalized == "1" || normalized == "ON"
        || normalized == "TRUE" || normalized == "YES") {
        value = true;
        return true;
    }
    if (normalized == "0" || normalized == "OFF"
        || normalized == "FALSE" || normalized == "NO") {
        value = false;
        return true;
    }
    return false;
}

bool parseCrop(const std::string& text, ImageCrop& crop)
{
    std::vector<std::string> values;
    size_t begin = 0;
    while (begin <= text.size()) {
        const size_t end = text.find(':', begin);
        values.push_back(text.substr(begin, end - begin));
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
    return values.size() == 4
           && parseUnsigned(values[0], 0, 16383, crop.x)
           && parseUnsigned(values[1], 0, 16383, crop.y)
           && parseUnsigned(values[2], 1, 16384, crop.w)
           && parseUnsigned(values[3], 1, 16384, crop.h);
}

bool parseColor(const std::string& text, uint32_t& color)
{
    if (text.empty() || text[0] == '-')
        return false;
    char* end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(text.c_str(), &end, 0);
    if (errno != 0 || !end || *end != '\0'
        || parsed > std::numeric_limits<uint32_t>::max()) {
        return false;
    }
    color = static_cast<uint32_t>(parsed);
    return true;
}

bool parseCover(const std::string& text, CoverOptions& cover)
{
    std::vector<std::string> values;
    size_t begin = 0;
    while (begin <= text.size()) {
        const size_t end = text.find(':', begin);
        values.push_back(text.substr(begin, end - begin));
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
    return values.size() == 5
           && parseUnsigned(values[0], 0, 16383, cover.rect.x)
           && parseUnsigned(values[1], 0, 16383, cover.rect.y)
           && parseUnsigned(values[2], 1, 16384, cover.rect.w)
           && parseUnsigned(values[3], 1, 16384, cover.rect.h)
           && parseColor(values[4], cover.color);
}

bool parseUnitFloat(const std::string& text, float& value)
{
    if (text.empty())
        return false;
    char* end = nullptr;
    errno = 0;
    const float parsed = std::strtof(text.c_str(), &end);
    if (errno != 0 || !end || *end != '\0' || !std::isfinite(parsed)
        || parsed < 0.0f || parsed > 1.0f) {
        return false;
    }
    value = parsed;
    return true;
}

bool parseBlendAlphaMode(const std::string& text,
                         BlendAlphaMode& mode)
{
    const std::string value = uppercase(text);
    if (value == "OPAQUE")
        mode = BlendAlphaMode::Opaque;
    else if (value == "STRAIGHT")
        mode = BlendAlphaMode::Straight;
    else if (value == "PREMULTIPLIED" || value == "PREMULT")
        mode = BlendAlphaMode::Premultiplied;
    else
        return false;
    return true;
}

const char* blendAlphaModeName(BlendAlphaMode mode)
{
    switch (mode) {
        case BlendAlphaMode::Opaque:
            return "opaque";
        case BlendAlphaMode::Premultiplied:
            return "premultiplied";
        default:
            return "straight";
    }
}

bool parseBlend(const std::string& text, BlendOptions& blend)
{
    std::vector<std::string> values;
    size_t begin = 0;
    while (begin <= text.size()) {
        const size_t end = text.find(':', begin);
        values.push_back(text.substr(begin, end - begin));
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
    if (values.size() < 5 || values.size() > 7
        || !parseUnsigned(values[0], 0, 16383, blend.rect.x)
        || !parseUnsigned(values[1], 0, 16383, blend.rect.y)
        || !parseUnsigned(values[2], 1, 16384, blend.rect.w)
        || !parseUnsigned(values[3], 1, 16384, blend.rect.h)
        || !parseColor(values[4], blend.color)) {
        return false;
    }
    if (values.size() >= 6 && !parseUnitFloat(values[5], blend.opacity))
        return false;
    return values.size() < 7
           || parseBlendAlphaMode(values[6], blend.alpha_mode);
}

bool parseRotation(const std::string& text, int32_t& rotation)
{
    if (text == "0")
        rotation = 0;
    else if (text == "90")
        rotation = 90;
    else if (text == "180")
        rotation = 180;
    else if (text == "270")
        rotation = 270;
    else
        return false;
    return true;
}

void printUsage(const char* program)
{
    std::cout
        << "Usage: " << program << " [options]\n\n"
        << "Image parameters:\n"
        << "  -f, --input-format FORMAT     Input format (default RGB24)\n"
        << "  -s, --input-size WxH          Input visible size\n"
        << "  -w, --input-width N           Input visible width\n"
        << "  -h, --input-height N          Input visible height\n"
        << "  -p, --input-stride HxV        Input stride\n"
        << "  -a, --input-afbc 0|1          Input linear/AFBC\n"
        << "  -F, --output-format FORMAT    Output format\n"
        << "  -S, --output-size WxH         Output visible size\n"
        << "  -W, --output-width N          Output visible width\n"
        << "  -H, --output-height N         Output visible height\n"
        << "  -P, --output-stride HxV       Output stride\n"
        << "  -A, --output-afbc 0|1         Output linear/AFBC\n\n"
        << "Processing parameters:\n"
        << "  -c, --crop X:Y:W:H            Input crop; omit for full image\n"
        << "  -r, --rotation 0|90|180|270   Image rotation\n"
        << "  -m, --mirror 0|1              Horizontal mirror\n"
        << "  -v, --flip 0|1                Vertical flip\n"
        << "  -k, --cover X:Y:W:H:ARGB      Output cover; repeatable (max 16)\n"
        << "  -b, --blend X:Y:W:H:ARGB[:OPACITY[:ALPHA_MODE]]\n"
        << "                                  Solid RGBA image layer; repeatable (max 64)\n"
        << "                                  ALPHA_MODE: opaque|straight|premultiplied\n"
        << "  -C, --change-output 0|1       Configure output through parameters\n\n"
        << "File parameters:\n"
        << "  -i, --input-file FILE         Read input buffer storage\n"
        << "  -o, --output-file FILE        Write final output buffer storage\n\n"
        << "Benchmark parameters:\n"
        << "  -u, --warmup N                Warm-up frames (default 20)\n"
        << "  -n, --iterations N            Measured frames (default 300)\n"
        << "\nDisplay parameters (ModuleAppSource -> ModuleRendererVideo):\n"
        << "  -d, --display MODE            none|input|output|both\n"
        << "  -t, --display-duration SEC    Display seconds; 0 waits for signal\n"
        << "  -R, --display-window WxH      Window size (default 640x360)\n"
        << "  -?, --help                    Print this help\n\n"
        << "File and input generation notes:\n"
        << "  -i/-o files contain the complete buffer storage selected by the\n"
        << "  format, stride and compression parameters. If an input file is\n"
        << "  larger than one buffer, only the first buffer is read.\n"
        << "  If synthetic content cannot be converted to the input format,\n"
        << "  generation is skipped and the allocated buffer is still passed to\n"
        << "  ModuleImageProcessor for actual capability testing.\n\n"
        << "Example:\n  " << program
        << " -f RGBA32 -s 3840x2160 -p 3840x2160 -a 0"
           " -F NV12 -S 1920x1080 -P 1920x1080 -A 0 -n 1000\n  "
        << program
        << " -f RGB24 -s 1920x1080 -F NV12 -S 1280x720 -n 10"
           " --display both --display-duration 10\n  "
        << program
        << " -f RGBA32 -s 640x360 -F NV12 -S 640x360"
           " --blend 176:104:288:144:0x80dc1464:0.5:straight -n 30\n";
}

bool parseOptions(int argc, char** argv, Options& options, bool& show_help,
                  std::string& error)
{
    for (int index = 1; index < argc; ++index) {
        const std::string option(argv[index]);
        if (option == "-?" || option == "--help") {
            show_help = true;
            continue;
        }
        if (option.find("--display=") == 0
            || option.find("--display-duration=") == 0
            || option.find("--display-window=") == 0) {
            continue;
        }
        if (index + 1 >= argc) {
            error = "missing value for " + option;
            return false;
        }
        const std::string value(argv[++index]);
        uint32_t parsed = 0;
        bool valid = true;
        if (option == "-f" || option == "--input-format")
            options.input.format = value;
        else if (option == "-i" || option == "--input-file")
            options.input_file = value;
        else if (option == "-o" || option == "--output-file")
            options.output_file = value;
        else if (option == "-s" || option == "--input-size")
            valid = parseSize(value, options.input.width, options.input.height);
        else if (option == "-p" || option == "--input-stride")
            valid = parseSize(value, options.input.hstride,
                              options.input.vstride);
        else if (option == "-w" || option == "--input-width") {
            valid = parseUnsigned(value, 1, 16384, parsed);
            options.input.width = parsed;
        } else if (option == "-h" || option == "--input-height") {
            valid = parseUnsigned(value, 1, 16384, parsed);
            options.input.height = parsed;
        } else if (option == "-a" || option == "--input-afbc")
            valid = parseAfbc(value, options.input.afbc);
        else if (option == "-F" || option == "--output-format") {
            options.output.format = value;
            options.output_format_set = true;
        } else if (option == "-S" || option == "--output-size") {
            valid = parseSize(value, options.output.width, options.output.height);
            options.output_width_set = true;
            options.output_height_set = true;
        } else if (option == "-P" || option == "--output-stride") {
            valid = parseSize(value, options.output.hstride,
                              options.output.vstride);
        } else if (option == "-W" || option == "--output-width") {
            valid = parseUnsigned(value, 1, 16384, parsed);
            options.output.width = parsed;
            options.output_width_set = true;
        } else if (option == "-H" || option == "--output-height") {
            valid = parseUnsigned(value, 1, 16384, parsed);
            options.output.height = parsed;
            options.output_height_set = true;
        } else if (option == "-A" || option == "--output-afbc") {
            valid = parseAfbc(value, options.output.afbc);
            options.output_afbc_set = true;
        } else if (option == "-c" || option == "--crop") {
            valid = parseCrop(value, options.crop);
            options.crop_set = true;
        } else if (option == "-r" || option == "--rotation") {
            valid = parseRotation(value, options.rotation);
        } else if (option == "-m" || option == "--mirror") {
            valid = parseBoolean(value, options.mirror);
        } else if (option == "-v" || option == "--flip") {
            valid = parseBoolean(value, options.flip);
        } else if (option == "-k" || option == "--cover") {
            CoverOptions cover;
            valid = options.covers.size() < 16
                    && parseCover(value, cover);
            if (valid)
                options.covers.push_back(cover);
        } else if (option == "-b" || option == "--blend") {
            BlendOptions blend;
            valid = options.blends.size() < 64
                    && parseBlend(value, blend);
            if (valid)
                options.blends.push_back(blend);
        } else if (option == "-C" || option == "--change-output") {
            valid = parseBoolean(value, options.change_output);
        } else if (option == "-u" || option == "--warmup") {
            valid = parseUnsigned(value, 0, 1000000, parsed);
            options.warmup = parsed;
        } else if (option == "-n" || option == "--iterations") {
            valid = parseUnsigned(value, 1, 1000000, parsed);
            options.iterations = parsed;
        } else if (option == "--display"
                   || option == "--display-duration"
                   || option == "--display-window") {
            // Long-form common test options are parsed through CliOptions.
            continue;
        } else if (option == "-d") {
            valid = parseDisplayMode(value, options.display);
        } else if (option == "-t") {
            valid = parseUnsigned(value, 0, 86400, parsed);
            options.display_duration = parsed;
        } else if (option == "-R") {
            valid = parseSize(value, options.display_width,
                              options.display_height);
        } else {
            error = "unknown option: " + option;
            return false;
        }
        if (!valid) {
            error = "invalid value for " + option + ": " + value;
            return false;
        }
    }
    return true;
}

bool applyCommonTestOptions(const CliOptions& cli, Options& options,
                            std::string& error)
{
    uint32_t parsed = 0;
    if (cli.has("display")
        && !parseDisplayMode(cli.get("display"), options.display)) {
        error = "invalid value for --display: " + cli.get("display");
        return false;
    }
    if (cli.has("display-duration")) {
        const std::string value = cli.get("display-duration");
        if (!parseUnsigned(value, 0, 86400, parsed)) {
            error = "invalid value for --display-duration: " + value;
            return false;
        }
        options.display_duration = parsed;
    }
    if (cli.has("display-window")) {
        const std::string value = cli.get("display-window");
        if (!parseSize(value, options.display_width, options.display_height)) {
            error = "invalid value for --display-window: " + value;
            return false;
        }
    }
    return true;
}

void inheritOutputOptions(Options& options)
{
    if (!options.output_format_set)
        options.output.format = options.input.format;
    if (!options.output_width_set)
        options.output.width = options.input.width;
    if (!options.output_height_set)
        options.output.height = options.input.height;
    if (!options.output_afbc_set)
        options.output.afbc = options.input.afbc;
}

bool validateImageOptions(const char* role, ImageOptions& options,
                          uint32_t& format, std::string& error)
{
    format = v4l2GetFmtByName(options.format.c_str());
    if (format == 0) {
        error = std::string("unsupported ") + role + " format: "
                + options.format;
        return false;
    }
    options.format = v4l2GetFmtName(format);
    if (options.hstride == 0) {
        options.hstride = options.afbc ? ALIGN(options.width, 16)
                                       : options.width;
    }
    if (options.vstride == 0) {
        options.vstride = options.afbc ? ALIGN(options.height, 16)
                                       : options.height;
    }
    if (options.hstride < options.width || options.vstride < options.height) {
        error = std::string(role) + " stride "
                + std::to_string(options.hstride) + "x"
                + std::to_string(options.vstride)
                + " is smaller than visible size "
                + std::to_string(options.width) + "x"
                + std::to_string(options.height);
        return false;
    }
    if (options.afbc
        && ((options.hstride & 15U) != 0
            || (options.vstride & 15U) != 0)) {
        error = std::string(role)
                + " AFBC16x16 stride must be aligned to 16: "
                + std::to_string(options.hstride) + "x"
                + std::to_string(options.vstride);
        return false;
    }
    return true;
}

ImagePara makeImagePara(const ImageOptions& options, uint32_t format)
{
    return ImagePara(
        options.width, options.height, options.hstride, options.vstride, format,
        options.afbc ? ImageCompression::Afbc16x16
                     : ImageCompression::Linear);
}

const char* compressionName(const ImagePara& para)
{
    return para.compression == ImageCompression::Afbc16x16 ? "AFBC16x16"
                                                           : "linear";
}

std::shared_ptr<VideoBuffer> allocateBuffer(const ImagePara& para)
{
    auto buffer = std::make_shared<VideoBuffer>(VideoBuffer::DRM_BUFFER_CACHEABLE);
    buffer->allocBuffer(para);
    if (buffer->getBufFd() <= 0 || buffer->getSize() == 0
        || !buffer->getActiveData()) {
        return nullptr;
    }
    buffer->setMediaBufferType(BUFFER_TYPE_VIDEO);
    return buffer;
}

void fillSolidRgba32(const std::shared_ptr<VideoBuffer>& buffer,
                     uint32_t color)
{
    const ImagePara para = buffer->getImagePara();
    std::memset(buffer->getActiveData(), 0, buffer->getSize());
    auto* data = static_cast<uint8_t*>(buffer->getActiveData());
    const uint8_t red = static_cast<uint8_t>((color >> 16) & 0xffU);
    const uint8_t green = static_cast<uint8_t>((color >> 8) & 0xffU);
    const uint8_t blue = static_cast<uint8_t>(color & 0xffU);
    const uint8_t alpha = static_cast<uint8_t>((color >> 24) & 0xffU);
    for (uint32_t y = 0; y < para.height; ++y) {
        uint8_t* row = data + static_cast<size_t>(y) * para.hstride * 4;
        for (uint32_t x = 0; x < para.width; ++x) {
            row[x * 4] = red;
            row[x * 4 + 1] = green;
            row[x * 4 + 2] = blue;
            row[x * 4 + 3] = alpha;
        }
    }
    buffer->flushDrmBuf();
}

int createBlendInputs(const Options& options, const ImagePara& output_para,
                      std::vector<BlendInputBuffer>& inputs,
                      std::string& error)
{
    inputs.clear();
    inputs.reserve(options.blends.size());
    for (size_t index = 0; index < options.blends.size(); ++index) {
        const BlendOptions& blend = options.blends[index];
        if (blend.rect.x >= output_para.width
            || blend.rect.y >= output_para.height
            || blend.rect.w > output_para.width - blend.rect.x
            || blend.rect.h > output_para.height - blend.rect.y) {
            error = "blend layer " + std::to_string(index)
                    + " exceeds output bounds";
            return -ERANGE;
        }
        const ImagePara layer_para(
            blend.rect.w, blend.rect.h, ALIGN(blend.rect.w, 16),
            ALIGN(blend.rect.h, 16), V4L2_PIX_FMT_RGB32);
        auto buffer = allocateBuffer(layer_para);
        if (!buffer) {
            error = "failed to allocate blend layer "
                    + std::to_string(index);
            return -ENOMEM;
        }
        fillSolidRgba32(buffer, blend.color);

        BlendInputBuffer input;
        input.input_id = static_cast<MediaChannelId>(index + 1);
        input.buffer = std::move(buffer);
        inputs.push_back(std::move(input));
    }
    return 0;
}

bool readBufferFromFile(const std::string& path,
                        const std::shared_ptr<VideoBuffer>& buffer,
                        std::string& error)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "failed to open input file: " + path;
        return false;
    }

    const std::streamoff file_size = file.tellg();
    const size_t buffer_size = buffer->getActiveSize();
    if (file_size < 0
        || static_cast<uint64_t>(file_size)
               < static_cast<uint64_t>(buffer_size)) {
        error = "input file is smaller than the allocated input buffer: "
                + std::to_string(file_size < 0 ? 0 : file_size) + " < "
                + std::to_string(buffer_size) + " bytes";
        return false;
    }

    file.seekg(0, std::ios::beg);
    file.read(static_cast<char*>(buffer->getActiveData()),
              static_cast<std::streamsize>(buffer_size));
    if (!file || static_cast<size_t>(file.gcount()) != buffer_size) {
        error = "failed to read " + std::to_string(buffer_size)
                + " bytes from input file: " + path;
        return false;
    }
    buffer->flushDrmBuf();

    std::cout << "[FILE] read " << buffer_size << " bytes from " << path;
    if (static_cast<uint64_t>(file_size)
        > static_cast<uint64_t>(buffer_size)) {
        std::cout << " (remaining data ignored)";
    }
    std::cout << '\n';
    return true;
}

bool writeBufferToFile(const std::string& path,
                       const std::shared_ptr<VideoBuffer>& buffer,
                       std::string& error)
{
    if (!buffer || !buffer->getActiveData() || buffer->getActiveSize() == 0) {
        error = "output buffer is empty";
        return false;
    }

    buffer->invalidateDrmBuf();
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "failed to open output file: " + path;
        return false;
    }

    const size_t buffer_size = buffer->getActiveSize();
    file.write(static_cast<const char*>(buffer->getActiveData()),
               static_cast<std::streamsize>(buffer_size));
    if (!file) {
        error = "failed to write " + std::to_string(buffer_size)
                + " bytes to output file: " + path;
        return false;
    }
    std::cout << "[FILE] wrote " << buffer_size << " bytes to " << path
              << '\n';
    return true;
}
void fillRgba32Pattern(const std::shared_ptr<VideoBuffer>& buffer)
{
    const ImagePara para = buffer->getImagePara();
    std::memset(buffer->getActiveData(), 0, buffer->getSize());
    auto* data = static_cast<uint8_t*>(buffer->getActiveData());
    const uint32_t x_range = para.width > 1 ? para.width - 1 : 1;
    const uint32_t y_range = para.height > 1 ? para.height - 1 : 1;
    const uint32_t diagonal_range = x_range + y_range;
    for (uint32_t y = 0; y < para.height; ++y) {
        uint8_t* row = data + static_cast<size_t>(y) * para.hstride * 4;
        for (uint32_t x = 0; x < para.width; ++x) {
            row[x * 4] = static_cast<uint8_t>(17 + 190 * x / x_range);
            row[x * 4 + 1] = static_cast<uint8_t>(31 + 180 * y / y_range);
            row[x * 4 + 2] = static_cast<uint8_t>(
                47 + 160 * (x + y) / diagonal_range);
            row[x * 4 + 3] = 0xff;
        }
    }
    buffer->flushDrmBuf();
}

class BenchmarkModuleImageProcessor : public ModuleImageProcessor
{
public:
    BenchmarkModuleImageProcessor(const ImagePara& input,
                                  const ImagePara& output)
        : ModuleImageProcessor(input, output)
    {
    }

    ~BenchmarkModuleImageProcessor() override
    {
        if (context_current_)
            teardown();
    }

    int prepare()
    {
        const int result = init();
        if (result < 0)
            return result;
        context_current_ = setup();
        return context_current_ ? 0 : -EIO;
    }

    int process(const std::shared_ptr<VideoBuffer>& input,
                const std::shared_ptr<VideoBuffer>& output)
    {
        MediaBufferContext context;
        context.buffer = input;
        context.input_id = 0;
        std::shared_ptr<MediaBuffer> media_output = output;
        return static_cast<int>(doConsume(context, media_output));
    }

    int cacheBlendInput(MediaChannelId input_id,
                        const std::shared_ptr<VideoBuffer>& input)
    {
        MediaBufferContext context;
        context.buffer = input;
        context.input_id = input_id;
        std::shared_ptr<MediaBuffer> no_output;
        const ConsumeResult result = doConsume(context, no_output);
        return result == CONSUME_SKIP ? 0 : static_cast<int>(result);
    }

private:
    bool context_current_ = false;
};

int configureProcessor(BenchmarkModuleImageProcessor& processor,
                       const Options& options)
{
    const ImageCrop crop = options.crop_set ? options.crop : ImageCrop{};
    const int result = processor.setParameter(
        "transform",
        ParameterObject({
            {"crop",
             {{"x", static_cast<int64_t>(crop.x)},
              {"y", static_cast<int64_t>(crop.y)},
              {"width", static_cast<int64_t>(crop.w)},
              {"height", static_cast<int64_t>(crop.h)}}},
            {"rotation", static_cast<int>(options.rotation)},
            {"mirror", options.mirror},
            {"flip", options.flip},
        }));
    if (result < 0)
        return result;
    for (size_t index = 0; index < options.covers.size(); ++index) {
        const CoverOptions& cover = options.covers[index];
        const int cover_result = processor.setParameter(
            "cover",
            ParameterObject({
                {"index", static_cast<int64_t>(index)},
                {"crop",
                 {{"x", static_cast<int64_t>(cover.rect.x)},
                  {"y", static_cast<int64_t>(cover.rect.y)},
                  {"width", static_cast<int64_t>(cover.rect.w)},
                  {"height", static_cast<int64_t>(cover.rect.h)}}},
                {"color", static_cast<int64_t>(cover.color)},
            }));
        if (cover_result < 0)
            return cover_result;
    }
    return 0;
}

ParameterObject imageParaParameters(const ImagePara& para)
{
    return ParameterObject({
        {"width", static_cast<int64_t>(para.width)},
        {"height", static_cast<int64_t>(para.height)},
        {"hstride", static_cast<int64_t>(para.hstride)},
        {"vstride", static_cast<int64_t>(para.vstride)},
        {"format", static_cast<int64_t>(para.v4l2Fmt)},
        {"compression", static_cast<int>(para.compression)},
    });
}

ParameterObject cropParameters(const ImageCrop& crop)
{
    return ParameterObject({
        {"x", static_cast<int64_t>(crop.x)},
        {"y", static_cast<int64_t>(crop.y)},
        {"width", static_cast<int64_t>(crop.w)},
        {"height", static_cast<int64_t>(crop.h)},
    });
}

int configureBlendInputs(BenchmarkModuleImageProcessor& processor,
                         const Options& options,
                         const std::vector<BlendInputBuffer>& inputs)
{
    if (options.blends.size() != inputs.size())
        return -EINVAL;
    for (size_t index = 0; index < inputs.size(); ++index) {
        const BlendOptions& blend = options.blends[index];
        const int result = processor.setParameter(
            "blend",
            ParameterObject({
                {"input-id", static_cast<int64_t>(inputs[index].input_id)},
                {"enabled", true},
                {"z-order", static_cast<int64_t>(index)},
                {"source-crop", cropParameters(ImageCrop{})},
                {"destination-rect", cropParameters(blend.rect)},
                {"opacity", static_cast<double>(blend.opacity)},
                {"rotation", 0},
                {"mirror", false},
                {"flip", false},
                {"alpha-mode", static_cast<int>(blend.alpha_mode)},
            }));
        if (result < 0)
            return result;
    }
    const auto& requirements = processor.getInputMediaChannelRequirements();
    for (const BlendInputBuffer& input : inputs) {
        const auto found = std::find_if(
            requirements.begin(), requirements.end(),
            [&input](const MediaChannelRequirement& requirement) {
                return requirement.input_id == input.input_id
                       && requirement.media_type == BUFFER_TYPE_VIDEO;
            });
        if (found == requirements.end())
            return -ENOENT;
    }
    return 0;
}

int convertBuffer(const std::shared_ptr<VideoBuffer>& source,
                  const std::shared_ptr<VideoBuffer>& destination)
{
    BenchmarkModuleImageProcessor gpu(source->getImagePara(),
                                      destination->getImagePara());
    int result = gpu.setParameter(
        "buffer-type", VideoBuffer::DRM_BUFFER_CACHEABLE);
    if (result == 0)
        result = gpu.prepare();
    return result < 0 ? result : gpu.process(source, destination);
}

std::shared_ptr<VideoBuffer> createInput(const ImagePara& input_para,
                                         bool& content_generated, int& error)
{
    auto input = allocateBuffer(input_para);
    if (!input) {
        error = -ENOMEM;
        return nullptr;
    }

    if (!content_generated) {
        std::memset(input->getActiveData(), 0, input->getSize());
        input->flushDrmBuf();
        error = 0;
        return input;
    }

    const ImagePara rgb_para(input_para.width, input_para.height,
                             ALIGN(input_para.width, 16),
                             ALIGN(input_para.height, 16),
                             V4L2_PIX_FMT_RGB32);
    if (input_para == rgb_para) {
        fillRgba32Pattern(input);
        error = 0;
        return input;
    }

    auto rgb = allocateBuffer(rgb_para);
    if (!rgb) {
        content_generated = false;
        std::memset(input->getActiveData(), 0, input->getSize());
        input->flushDrmBuf();
        error = 0;
        return input;
    }
    fillRgba32Pattern(rgb);

    const int convert_error = convertBuffer(rgb, input);
    if (convert_error != 0) {
        content_generated = false;
        std::memset(input->getActiveData(), 0, input->getSize());
        input->flushDrmBuf();
    }
    error = 0;
    return input;
}

int runBenchmark(const Options& options,
                 const std::shared_ptr<VideoBuffer>& input,
                 const ImagePara& output_para,
                 const std::vector<BlendInputBuffer>& blend_inputs,
                 BenchmarkResult& benchmark)
{
    BenchmarkModuleImageProcessor gpu(
        input->getImagePara(),
        options.change_output ? input->getImagePara() : output_para);
    int result = gpu.setParameter(
        "buffer-type", VideoBuffer::DRM_BUFFER_CACHEABLE);
    if (result == 0)
        result = configureProcessor(gpu, options);
    if (result == 0 && options.change_output)
        result = gpu.setParameter("output",
                                  imageParaParameters(output_para));
    if (result == 0)
        result = configureBlendInputs(gpu, options, blend_inputs);
    if (result == 0)
        result = gpu.prepare();
    if (result < 0)
        return result;
    for (size_t index = 0; index < blend_inputs.size(); ++index) {
        blend_inputs[index].buffer->setPUstimestamp(
            -static_cast<int64_t>(index + 1));
        result = gpu.cacheBlendInput(blend_inputs[index].input_id,
                                     blend_inputs[index].buffer);
        if (result != 0)
            return result;
    }
    if (!blend_inputs.empty()) {
        std::cout << "[BLEND] cached " << blend_inputs.size()
                  << " auxiliary frame(s); warmup and benchmark submit only "
                     "main input 0\n";
    }
    auto output = allocateBuffer(output_para);
    if (!output)
        return -ENOMEM;

    for (uint32_t index = 0; index < options.warmup; ++index) {
        input->setPUstimestamp(static_cast<int64_t>(index) * 1000);
        result = gpu.process(input, output);
        if (result != 0)
            return result;
    }

    benchmark.latency_ms.clear();
    benchmark.latency_ms.reserve(options.iterations);
    const auto total_start = std::chrono::steady_clock::now();
    for (uint32_t index = 0; index < options.iterations; ++index) {
        input->setPUstimestamp(
            static_cast<int64_t>(options.warmup + index) * 1000);
        const auto frame_start = std::chrono::steady_clock::now();
        result = gpu.process(input, output);
        const auto frame_end = std::chrono::steady_clock::now();
        if (result != 0)
            return result;
        benchmark.latency_ms.push_back(
            std::chrono::duration<double, std::milli>(frame_end - frame_start)
                .count());
    }
    const auto total_end = std::chrono::steady_clock::now();
    benchmark.total_ms = std::chrono::duration<double, std::milli>(
                             total_end - total_start)
                             .count();
    benchmark.fps = options.iterations * 1000.0 / benchmark.total_ms;
    benchmark.output_mpixels = static_cast<double>(output_para.width)
                               * output_para.height * benchmark.fps
                               / 1000000.0;
    benchmark.last_output = output;
    const double bytes_per_frame = static_cast<double>(input->getSize())
                                   + (benchmark.last_output
                                          ? benchmark.last_output->getSize()
                                          : 0);
    benchmark.estimated_gib_per_second = bytes_per_frame * benchmark.fps
                                         / (1024.0 * 1024.0 * 1024.0);
    return benchmark.last_output ? 0 : -EIO;
}

double percentile(const std::vector<double>& sorted, double fraction)
{
    if (sorted.empty())
        return 0.0;
    const double position = fraction * static_cast<double>(sorted.size() - 1);
    const size_t lower = static_cast<size_t>(position);
    const size_t upper = std::min(lower + 1, sorted.size() - 1);
    const double weight = position - static_cast<double>(lower);
    return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
}

void printBenchmark(const Options& options, const ImagePara& input_para,
                    const ImagePara& output_para,
                    const BenchmarkResult& benchmark)
{
    std::vector<double> sorted = benchmark.latency_ms;
    std::sort(sorted.begin(), sorted.end());
    const double average = std::accumulate(sorted.begin(), sorted.end(), 0.0)
                           / sorted.size();

    std::cout << "[PERF] ModuleImageProcessor direct processing\n"
              << "  input:  " << options.input.format << ' '
              << input_para.width << 'x' << input_para.height << ", stride="
              << input_para.hstride << 'x' << input_para.vstride << ", "
              << compressionName(input_para) << '\n'
              << "  output: " << options.output.format << ' '
              << output_para.width << 'x' << output_para.height << ", stride="
              << output_para.hstride << 'x' << output_para.vstride << ", "
              << compressionName(output_para) << '\n'
              << "  frames: warmup=" << options.warmup
              << ", measured=" << options.iterations << '\n'
              << std::fixed << std::setprecision(3)
              << "  total:  " << benchmark.total_ms << " ms\n"
              << "  rate:   " << benchmark.fps << " fps, "
              << benchmark.output_mpixels << " output MPix/s\n"
              << "  latency(ms): min=" << sorted.front()
              << ", avg=" << average
              << ", p50=" << percentile(sorted, 0.50)
              << ", p90=" << percentile(sorted, 0.90)
              << ", p95=" << percentile(sorted, 0.95)
              << ", p99=" << percentile(sorted, 0.99)
              << ", max=" << sorted.back() << '\n'
              << "  estimated DMA traffic: "
              << benchmark.estimated_gib_per_second
              << " GiB/s (input+output buffer sizes; not measured DDR traffic)\n";
}

struct ContentValidation {
    uint64_t checksum = 0;
    double mean_absolute_error = 0.0;
    int maximum_error = 0;
    double high_error_pixel_ratio = 0.0;
    uint64_t blend_changed_pixel_writes = 0;
};

uint8_t clampColor(float value)
{
    return static_cast<uint8_t>(std::max(
        0L, std::min(255L, std::lround(value))));
}

uint64_t applyCpuBlendReference(
    const std::shared_ptr<VideoBuffer>& reference,
    const std::vector<BlendOptions>& blends)
{
    if (blends.empty())
        return 0;

    const ImagePara para = reference->getImagePara();
    auto* data = static_cast<uint8_t*>(reference->getActiveData());
    uint64_t changed_pixel_writes = 0;
    for (const BlendOptions& blend : blends) {
        const float opacity = blend.opacity;
        const float source_alpha = static_cast<float>(
                                       (blend.color >> 24) & 0xffU)
                                   / 255.0f;
        const float alpha = blend.alpha_mode == BlendAlphaMode::Opaque
                                ? opacity
                                : source_alpha * opacity;
        const float inverse_alpha = 1.0f - alpha;
        const uint8_t source[3] = {
            static_cast<uint8_t>((blend.color >> 16) & 0xffU),
            static_cast<uint8_t>((blend.color >> 8) & 0xffU),
            static_cast<uint8_t>(blend.color & 0xffU),
        };
        for (uint32_t y = blend.rect.y;
             y < blend.rect.y + blend.rect.h; ++y) {
            uint8_t* row = data + static_cast<size_t>(y) * para.hstride * 4;
            for (uint32_t x = blend.rect.x;
                 x < blend.rect.x + blend.rect.w; ++x) {
                uint8_t* pixel = row + static_cast<size_t>(x) * 4;
                bool changed = false;
                for (size_t channel = 0; channel < 3; ++channel) {
                    const float source_contribution = blend.alpha_mode == BlendAlphaMode::Premultiplied
                                                          ? static_cast<float>(source[channel]) * opacity
                                                          : static_cast<float>(source[channel]) * alpha;
                    const uint8_t value = clampColor(
                        source_contribution
                        + static_cast<float>(pixel[channel]) * inverse_alpha);
                    changed = changed || value != pixel[channel];
                    pixel[channel] = value;
                }
                if (changed)
                    ++changed_pixel_writes;
            }
        }
    }
    return changed_pixel_writes;
}

int verifyOutput(const std::shared_ptr<VideoBuffer>& input,
                 const std::shared_ptr<VideoBuffer>& output,
                 const Options& options, bool require_nonzero,
                 ContentValidation& validation)
{
    if (!input || !output)
        return -EINVAL;
    const ImagePara output_para = output->getImagePara();
    const ImagePara rgb_para(output_para.width, output_para.height,
                             ALIGN(output_para.width, 16),
                             ALIGN(output_para.height, 16),
                             V4L2_PIX_FMT_RGB32);
    auto actual = allocateBuffer(rgb_para);
    auto reference = allocateBuffer(rgb_para);
    if (!actual || !reference)
        return -ENOMEM;
    int result = convertBuffer(output, actual);
    if (result != 0)
        return result;
    BenchmarkModuleImageProcessor reference_processor(input->getImagePara(),
                                                      rgb_para);
    result = reference_processor.setParameter(
        "buffer-type", VideoBuffer::DRM_BUFFER_CACHEABLE);
    if (result == 0)
        result = configureProcessor(reference_processor, options);
    if (result == 0)
        result = reference_processor.prepare();
    if (result == 0)
        result = reference_processor.process(input, reference);
    if (result != 0)
        return result;

    actual->invalidateDrmBuf();
    reference->invalidateDrmBuf();
    validation = ContentValidation();
    validation.blend_changed_pixel_writes = applyCpuBlendReference(
        reference, options.blends);
    const auto* actual_data = static_cast<const uint8_t*>(actual->getActiveData());
    const auto* reference_data = static_cast<const uint8_t*>(reference->getActiveData());
    if (!actual_data || !reference_data)
        return -EFAULT;

    validation.checksum = 1469598103934665603ULL;
    bool nonzero = false;
    uint64_t total_error = 0;
    uint64_t compared_channels = 0;
    uint64_t high_error_pixels = 0;
    const size_t row_bytes = static_cast<size_t>(rgb_para.hstride) * 4;
    for (uint32_t y = 0; y < rgb_para.height; ++y) {
        const uint8_t* actual_row = actual_data + static_cast<size_t>(y) * row_bytes;
        const uint8_t* reference_row = reference_data
                                       + static_cast<size_t>(y) * row_bytes;
        for (uint32_t x = 0; x < rgb_para.width; ++x) {
            int pixel_error = 0;
            for (size_t channel = 0; channel < 3; ++channel) {
                const size_t offset = static_cast<size_t>(x) * 4 + channel;
                const int error = std::abs(
                    static_cast<int>(actual_row[offset])
                    - static_cast<int>(reference_row[offset]));
                nonzero = nonzero || actual_row[offset] != 0;
                total_error += error;
                ++compared_channels;
                pixel_error = std::max(pixel_error, error);
                validation.maximum_error = std::max(validation.maximum_error, error);
                validation.checksum ^= actual_row[offset];
                validation.checksum *= 1099511628211ULL;
            }
            if (pixel_error > 48)
                ++high_error_pixels;
        }
    }
    const uint64_t pixel_count = static_cast<uint64_t>(rgb_para.width)
                                 * rgb_para.height;
    validation.mean_absolute_error = compared_channels == 0
                                         ? 0.0
                                         : static_cast<double>(total_error)
                                               / compared_channels;
    validation.high_error_pixel_ratio = pixel_count == 0
                                            ? 0.0
                                            : static_cast<double>(
                                                  high_error_pixels)
                                                  / pixel_count;

    if (require_nonzero && !nonzero)
        return -EIO;
    if ((require_nonzero || !options.blends.empty())
        && (validation.mean_absolute_error > 10.0
            || validation.high_error_pixel_ratio > 0.01)) {
        return -EILSEQ;
    }
    return 0;
}

struct DisplayView {
    std::string name;
    std::shared_ptr<VideoBuffer> buffer;
    std::shared_ptr<ModuleAppSource> source;
    std::shared_ptr<ModuleRendererVideo> renderer;
};

int addDisplayView(const Options& options, const std::string& name,
                   const std::shared_ptr<VideoBuffer>& buffer,
                   size_t window_index, RunMonitor& monitor,
                   std::vector<DisplayView>& views)
{
    if (!buffer)
        return -EINVAL;

    MediaChannelInfo channel;
    channel.id = 0;
    channel.name = name;
    channel.media_type = BUFFER_TYPE_VIDEO;
    channel.codec = MEDIA_CODEC_VIDEO_RAW;
    channel.image_para = buffer->getImagePara();

    AppSourceOptions source_options;
    source_options.queue_capacity = 2;
    source_options.queue_policy = AppQueuePolicy::DROP_OLDEST;
    auto source = std::make_shared<ModuleAppSource>(
        std::vector<MediaChannelInfo>{channel}, source_options);
    int result = source->init();
    if (result < 0)
        return result;

    auto renderer = std::make_shared<ModuleRendererVideo>(
        channel.image_para, "ImageProcessor " + name);
    const int window_x = static_cast<int>(
        window_index * (static_cast<size_t>(options.display_width) + 16));
    result = renderer->setWindowRect(
        window_x, 0, options.display_width, options.display_height);
    if (result < 0)
        return result;
    result = renderer->connectProducer(source);
    if (result < 0)
        return result;
    result = renderer->init();
    if (result < 0)
        return result;

    renderer->setMediaStatusChangeHooker(
        [&monitor](const std::string& module_name, MediaStatus status) {
            monitor.onStatus(module_name, status);
        });
    renderer->setMediaBufferConsumeHooker(
        [&monitor](const std::string& module_name, int queue_size,
                   std::shared_ptr<MediaBuffer> frame) {
            monitor.onBuffer(module_name, queue_size, frame);
        });

    DisplayView view;
    view.name = name;
    view.buffer = buffer;
    view.source = source;
    view.renderer = renderer;
    views.push_back(std::move(view));
    return 0;
}

int runDisplay(const Options& options,
               const std::shared_ptr<VideoBuffer>& input,
               const std::shared_ptr<VideoBuffer>& output)
{
    if (options.display == DisplayMode::None)
        return 0;

    RunMonitor monitor(0, options.display_duration, 0, false);
    bool failed = false;
    std::vector<DisplayView> views;
    int result = 0;
    size_t window_index = 0;
    if (options.display == DisplayMode::Input
        || options.display == DisplayMode::Both) {
        result = addDisplayView(options, "input", input, window_index++, monitor,
                                views);
        if (result < 0)
            return result;
    }
    if (options.display == DisplayMode::Output
        || options.display == DisplayMode::Both) {
        result = addDisplayView(options, "output", output, window_index++,
                                monitor, views);
        if (result < 0)
            return result;
    }

    FFMediaTest::installSignalHandlers(monitor);
    monitor.reset();
    for (auto& view : views)
        view.source->start();

    std::cout << "[DISPLAY] PID=" << getpid()
              << ", mode=" << displayModeName(options.display)
              << ", views=" << views.size()
              << ", window=" << options.display_width << 'x'
              << options.display_height
              << ", duration=" << options.display_duration << "s\n"
              << "[DISPLAY] backend is selected by FFMEDIA_DISPLAY_BACKEND "
                 "(or auto-selected from WAYLAND_DISPLAY/DISPLAY); "
                 "press Ctrl+C to stop\n";

    for (auto& view : views) {
        view.buffer->setPUstimestamp(0);
        uint64_t ticket = 0;
        result = view.source->submit(view.buffer, 0, 1000, &ticket);
        if (result < 0) {
            std::cerr << "Display " << view.name
                      << " submit failed: " << result << '\n';
            failed = true;
            break;
        }
        result = view.source->wait(ticket, 2000);
        if (result < 0) {
            std::cerr << "Display " << view.name
                      << " frame wait failed: " << result << '\n';
            failed = true;
            break;
        }
    }
    if (!failed)
        monitor.wait();

    for (auto& view : views)
        view.source->stop();
    FFMediaTest::signalMonitor() = nullptr;
    monitor.printSummary("image processor display");
    std::cout << "[DISPLAY] stopped; one frame was submitted per view\n";
    return failed || monitor.abnormal() ? -EIO : 0;
}

}  // namespace

int main(int argc, char** argv)
{
    CliOptions cli(argc, argv);
    Options options;
    bool show_help = false;
    std::string error;
    if (!parseOptions(argc, argv, options, show_help, error)) {
        std::cerr << "Error: " << error << "\n\n";
        printUsage(argv[0]);
        return 2;
    }
    if (show_help) {
        printUsage(argv[0]);
        return 0;
    }
    if (!applyCommonTestOptions(cli, options, error)) {
        std::cerr << "Error: " << error << "\n\n";
        printUsage(argv[0]);
        return 2;
    }
    uint32_t input_format = 0;
    uint32_t output_format = 0;
    if (!validateImageOptions("input", options.input, input_format, error)) {
        std::cerr << "Error: " << error << '\n';
        return 2;
    }
    inheritOutputOptions(options);
    if (!validateImageOptions("output", options.output, output_format,
                              error)) {
        std::cerr << "Error: " << error << '\n';
        return 2;
    }

    const ImagePara input_para = makeImagePara(options.input, input_format);
    const ImagePara output_para = makeImagePara(options.output, output_format);
    const bool input_from_file = !options.input_file.empty();
    bool input_content_generated = !input_from_file;
    std::cout << "[CONFIG] input=" << options.input.format << ':'
              << options.input.width << 'x' << options.input.height << ':'
              << options.input.hstride << 'x' << options.input.vstride << ':'
              << compressionName(input_para) << ", output="
              << options.output.format << ':' << options.output.width << 'x'
              << options.output.height << ':' << options.output.hstride << 'x'
              << options.output.vstride << ':' << compressionName(output_para)
              << ", rotation="
              << options.rotation
              << ", mirror=" << options.mirror
              << ", flip=" << options.flip
              << ", covers=" << options.covers.size()
              << ", blends=" << options.blends.size()
              << ", change_output=" << options.change_output
              << ", warmup=" << options.warmup
              << ", iterations=" << options.iterations
              << ", display=" << displayModeName(options.display);
    if (options.display != DisplayMode::None) {
        std::cout << ':' << options.display_width << 'x'
                  << options.display_height << ':'
                  << options.display_duration << 's';
    }
    if (options.crop_set) {
        std::cout << ", crop=" << options.crop.x << ':' << options.crop.y
                  << ':' << options.crop.w << ':' << options.crop.h;
    }
    if (input_from_file)
        std::cout << ", input_file=" << options.input_file;
    if (!options.output_file.empty())
        std::cout << ", output_file=" << options.output_file;
    std::cout << '\n';
    for (size_t index = 0; index < options.blends.size(); ++index) {
        const BlendOptions& blend = options.blends[index];
        std::cout << "[BLEND] layer=" << index
                  << ", input=" << index + 1 << ", z-order=" << index
                  << ", rect="
                  << blend.rect.x << ':' << blend.rect.y << ':'
                  << blend.rect.w << ':' << blend.rect.h << ", color=0x"
                  << std::hex << std::setw(8) << std::setfill('0')
                  << blend.color << std::dec << std::setfill(' ')
                  << ", opacity=" << blend.opacity << ", alpha="
                  << blendAlphaModeName(blend.alpha_mode) << '\n';
    }
    int result = 0;
    std::shared_ptr<VideoBuffer> input;
    if (input_from_file) {
        input = allocateBuffer(input_para);
        if (input && !readBufferFromFile(options.input_file, input, error)) {
            std::cerr << "Failed to read input buffer: " << error << '\n';
            return 1;
        }
        if (!input)
            result = -ENOMEM;
    } else {
        input = createInput(input_para, input_content_generated, result);
    }
    if (!input) {
        std::cerr << "Failed to prepare input buffer, error=" << result << '\n';
        return 1;
    }
    if (!input_from_file && !input_content_generated) {
        std::cout << "[SKIP] synthetic input generation for "
                  << options.input.format
                  << "; continuing with allocated input buffer\n";
    }

    std::vector<BlendInputBuffer> blend_inputs;
    result = createBlendInputs(options, output_para, blend_inputs, error);
    if (result != 0) {
        std::cerr << "Failed to prepare blend layers: " << error
                  << ", error=" << result << '\n';
        return 1;
    }

    BenchmarkResult benchmark;
    result = runBenchmark(options, input, output_para, blend_inputs,
                          benchmark);
    if (result != 0) {
        std::cerr << "ModuleImageProcessor benchmark failed, error=" << result
                  << '\n';
        return 1;
    }
    printBenchmark(options, input_para, output_para, benchmark);

    const ImagePara actual = benchmark.last_output->getImagePara();
    if (!(actual == output_para)) {
        std::cerr << "Output metadata does not match requested parameters\n";
        return 1;
    }
    if (!options.output_file.empty()
        && !writeBufferToFile(options.output_file, benchmark.last_output,
                              error)) {
        std::cerr << "Failed to write output buffer: " << error << '\n';
        return 1;
    }
    ContentValidation validation;
    result = verifyOutput(input, benchmark.last_output, options,
                          input_content_generated, validation);
    if (result == 0 || result == -EILSEQ) {
        std::cout << std::fixed << std::setprecision(3)
                  << "[VERIFY] RGBA content comparison: MAE="
                  << validation.mean_absolute_error << ", max="
                  << validation.maximum_error << ", pixels(error>48)="
                  << validation.high_error_pixel_ratio * 100.0 << "%\n";
        if (!options.blends.empty()) {
            std::cout << "[VERIFY] CPU SourceOver blend reference: layers="
                      << options.blends.size() << ", changed-pixel-writes="
                      << validation.blend_changed_pixel_writes << '\n';
        }
    }
    if (result != 0) {
        std::cerr << "Output image content verification failed, error="
                  << result << '\n';
        return 1;
    }
    result = runDisplay(options, input, benchmark.last_output);
    if (result != 0) {
        std::cerr << "Module display pipeline failed, error=" << result
                  << '\n';
        return 1;
    }
    std::cout << "[PASS] output metadata and image content, RGBA32 checksum=0x"
              << std::hex << validation.checksum << std::dec;
    if (!input_from_file && !input_content_generated)
        std::cout << " (input content generation skipped)";
    std::cout << '\n';
    return 0;
}

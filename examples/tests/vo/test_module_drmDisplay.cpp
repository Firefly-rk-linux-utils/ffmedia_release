#include "module/vi/module_fileReader.hpp"
#include "module/vo/module_drmDisplay.hpp"
#include "module/vp/module_mppdec.hpp"

#include <getopt.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace FFMedia;
using Rect = ::FFMedia::FFMedia::Rect;

namespace
{

struct OptionalRect {
    bool set = false;
    Rect value;
};

enum AutoRect {
    AUTO_RECT_NONE,
    AUTO_RECT_FULL,
    AUTO_RECT_CENTER_HALF,
    AUTO_RECT_CENTER_THREE_QUARTERS,
};

struct ViewConfig {
    std::string group;
    int screen = 0;
    uint32_t connector = 0;
    uint32_t plane = 0;
    PLANE_TYPE plane_type = PLANE_TYPE_OVERLAY_OR_PRIMARY;
    uint32_t zpos = 0xff;
    uint32_t linear = 1;
    uint32_t format = V4L2_PIX_FMT_NV12;
    DrmDisplayPlane::PLANE_DISPLAY_MODE mode = DrmDisplayPlane::MULTI_WINDOW_DISPLAY;
    DrmDisplayPlane::LAYOUT_MODE layout = DrmDisplayPlane::ABSOLUTE_LAYOUT;
    uint32_t grid_w = 1;
    uint32_t grid_h = 1;
    OptionalRect plane_rect;
    OptionalRect window_rect;
    OptionalRect relative_rect;
    OptionalRect image_rect;
    AutoRect auto_plane_rect = AUTO_RECT_FULL;
    AutoRect auto_window_rect = AUTO_RECT_NONE;
    AutoRect auto_image_rect = AUTO_RECT_NONE;
    bool visible = true;
};

struct Options {
    std::string input;
    std::string scenario = "single-window";
    bool loop = false;
    bool dry_run = false;
    bool decoder_afbc = false;
    unsigned duration_seconds = 0;
    uint64_t frame_limit = 0;
    unsigned decoder_buffers = 20;
    std::vector<uint32_t> connectors;
    std::vector<uint32_t> planes;
    std::vector<uint32_t> zposes;
    std::vector<ViewConfig> views;
};

struct GroupRuntime {
    ViewConfig config;
    std::shared_ptr<DrmDisplayPlane> plane;
    size_t window_count = 0;
};

std::atomic_bool stop_requested(false);

void signalHandler(int)
{
    stop_requested = true;
}

void printUsage(const char* program)
{
    std::cout
        << "Usage: " << program << " [OPTIONS] FILE\n\n"
        << "Decode FILE with ModuleFileReader + ModuleMppDec and display it "
           "with ModuleDrmDisplay.\n\n"
        << "Scenarios:\n"
        << "  single-window  One zero-copy window on one plane (default)\n"
        << "  multi-window   Four windows sharing one 2x2 plane\n"
        << "  multi-plane    Full-screen base plane plus centered overlay plane\n"
        << "  multi-device   One window per repeated --connector (at least two)\n"
        << "  crop           Center-half input crop, full-screen output\n"
        << "  regions        Input crop + centered plane + centered window\n"
        << "  all            2x2 base plane + cropped overlay; add a second "
           "device when two connectors are supplied\n"
        << "  custom         Use one or more repeated --view specifications\n\n"
        << "Options:\n"
        << "  -s, --scenario NAME       Select a scenario\n"
        << "  -V, --view SPEC           Add a custom view (repeatable)\n"
        << "  -C, --connector ID        Connector for presets (repeatable)\n"
        << "  -p, --plane ID            Plane ID for presets (repeatable, 0=auto)\n"
        << "  -z, --zpos N              Plane zpos for presets (repeatable)\n"
        << "  -l, --loop                Loop the input file\n"
        << "  -d, --duration SEC        Stop after a bounded duration\n"
        << "  -f, --frames N            Stop after N decoded frames\n"
        << "  -b, --decoder-buffers N   Decoder buffer count (default 20)\n"
        << "      --decoder-afbc        Request AFBC 16x16 decoder output\n"
        << "      --dry-run             Validate and print configuration only\n"
        << "  -h, --help                Show this help\n\n"
        << "View SPEC is comma-separated key=value data. Supported keys:\n"
        << "  group,screen,connector,plane,type,zpos,linear,format,mode,layout,"
           "grid,plane-rect,window-rect,relative-rect,image-rect,visible\n"
        << "Values: type=auto|overlay|primary|cursor, mode=multi|single,\n"
        << "        layout=absolute|relative, grid=COLS:ROWS,\n"
        << "        rectangles=X:Y:W:H, format=NV12 (or another V4L2 name).\n\n"
        << "Examples:\n"
        << "  " << program
        << " --scenario multi-window --duration 10 sample.mp4\n"
        << "  " << program
        << " --scenario multi-device -C 225 -C 226 --duration 10 sample.mp4\n"
        << "  " << program
        << " --scenario custom --view 'group=p0,connector=225,plane=0,zpos=1,"
           "mode=multi,plane-rect=0:0:1920:1080,window-rect=0:0:960:540,"
           "image-rect=160:90:960:540' --view 'group=p0,connector=225,"
           "plane=0,zpos=1,mode=multi,plane-rect=0:0:1920:1080,"
           "window-rect=960:540:960:540' --duration 10 sample.mp4\n";
}

bool parseUnsigned(const std::string& text, uint64_t& value)
{
    if (text.empty() || text[0] == '-')
        return false;
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = strtoull(text.c_str(), &end, 0);
    if (errno != 0 || end == text.c_str() || *end != '\0')
        return false;
    value = parsed;
    return true;
}

bool parseUint32(const std::string& text, uint32_t& value)
{
    uint64_t parsed = 0;
    if (!parseUnsigned(text, parsed) || parsed > UINT32_MAX)
        return false;
    value = static_cast<uint32_t>(parsed);
    return true;
}

bool parseRect(const std::string& text, Rect& rect)
{
    std::stringstream stream(text);
    std::string part;
    uint32_t values[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < 4; ++i) {
        if (!std::getline(stream, part, ':') || !parseUint32(part, values[i]))
            return false;
    }
    if (std::getline(stream, part, ':') || values[2] == 0 || values[3] == 0)
        return false;
    rect.set(values[0], values[1], values[2], values[3]);
    return true;
}

bool parseGrid(const std::string& text, uint32_t& width, uint32_t& height)
{
    const size_t separator = text.find(':');
    if (separator == std::string::npos)
        return false;
    return parseUint32(text.substr(0, separator), width)
           && parseUint32(text.substr(separator + 1), height)
           && width > 0 && height > 0;
}

std::vector<std::string> split(const std::string& text, char separator)
{
    std::vector<std::string> result;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, separator))
        result.push_back(item);
    return result;
}

bool parsePlaneType(const std::string& text, PLANE_TYPE& type)
{
    if (text == "auto")
        type = PLANE_TYPE_OVERLAY_OR_PRIMARY;
    else if (text == "overlay")
        type = PLANE_TYPE_OVERLAY;
    else if (text == "primary")
        type = PLANE_TYPE_PRIMARY;
    else if (text == "cursor")
        type = PLANE_TYPE_CURSOR;
    else
        return false;
    return true;
}

bool parseBool(const std::string& text, bool& value)
{
    if (text == "1" || text == "true" || text == "yes")
        value = true;
    else if (text == "0" || text == "false" || text == "no")
        value = false;
    else
        return false;
    return true;
}

bool parseView(const std::string& specification, ViewConfig& view,
               std::string& error)
{
    const std::vector<std::string> fields = split(specification, ',');
    for (size_t i = 0; i < fields.size(); ++i) {
        const std::string& field = fields[i];
        const size_t equals = field.find('=');
        if (equals == std::string::npos || equals == 0
            || equals + 1 == field.size()) {
            error = "invalid view field: " + field;
            return false;
        }
        const std::string key = field.substr(0, equals);
        const std::string value = field.substr(equals + 1);
        uint32_t number = 0;

        if (key == "group") {
            view.group = value;
        } else if (key == "screen") {
            if (!parseUint32(value, number))
                goto InvalidValue;
            view.screen = static_cast<int>(number);
        } else if (key == "connector") {
            if (!parseUint32(value, view.connector))
                goto InvalidValue;
        } else if (key == "plane") {
            if (!parseUint32(value, view.plane))
                goto InvalidValue;
        } else if (key == "type") {
            if (!parsePlaneType(value, view.plane_type))
                goto InvalidValue;
        } else if (key == "zpos") {
            if (!parseUint32(value, view.zpos))
                goto InvalidValue;
        } else if (key == "linear") {
            if (!parseUint32(value, view.linear)
                || (view.linear != 0 && view.linear != 1))
                goto InvalidValue;
        } else if (key == "format") {
            view.format = v4l2GetFmtByName(value.c_str());
            if (view.format == 0)
                goto InvalidValue;
        } else if (key == "mode") {
            if (value == "multi")
                view.mode = DrmDisplayPlane::MULTI_WINDOW_DISPLAY;
            else if (value == "single")
                view.mode = DrmDisplayPlane::SINGLE_WINDOW_DISPLAY;
            else
                goto InvalidValue;
        } else if (key == "layout") {
            if (value == "absolute")
                view.layout = DrmDisplayPlane::ABSOLUTE_LAYOUT;
            else if (value == "relative")
                view.layout = DrmDisplayPlane::RELATIVE_LAYOUT;
            else
                goto InvalidValue;
        } else if (key == "grid") {
            if (!parseGrid(value, view.grid_w, view.grid_h))
                goto InvalidValue;
        } else if (key == "plane-rect") {
            if (!parseRect(value, view.plane_rect.value))
                goto InvalidValue;
            view.plane_rect.set = true;
            view.auto_plane_rect = AUTO_RECT_NONE;
        } else if (key == "window-rect") {
            if (!parseRect(value, view.window_rect.value))
                goto InvalidValue;
            view.window_rect.set = true;
        } else if (key == "relative-rect") {
            if (!parseRect(value, view.relative_rect.value))
                goto InvalidValue;
            view.relative_rect.set = true;
            view.layout = DrmDisplayPlane::RELATIVE_LAYOUT;
        } else if (key == "image-rect") {
            if (!parseRect(value, view.image_rect.value))
                goto InvalidValue;
            view.image_rect.set = true;
        } else if (key == "visible") {
            if (!parseBool(value, view.visible))
                goto InvalidValue;
        } else {
            error = "unknown view key: " + key;
            return false;
        }
        continue;

    InvalidValue:
        error = "invalid value for " + key + ": " + value;
        return false;
    }
    return true;
}

uint32_t selectValue(const std::vector<uint32_t>& values, size_t index,
                     uint32_t fallback)
{
    return index < values.size() ? values[index] : fallback;
}

ViewConfig presetView(const Options& options, const std::string& group,
                      size_t selector_index, uint32_t default_zpos)
{
    ViewConfig view;
    view.group = group;
    view.connector = selectValue(options.connectors, selector_index, 0);
    view.plane = selectValue(options.planes, selector_index, 0);
    view.zpos = selectValue(options.zposes, selector_index, default_zpos);
    return view;
}

void appendMultiWindowPreset(const Options& options,
                             std::vector<ViewConfig>& views)
{
    for (uint32_t y = 0; y < 2; ++y) {
        for (uint32_t x = 0; x < 2; ++x) {
            ViewConfig view = presetView(options, "base", 0, 0xff);
            view.layout = DrmDisplayPlane::RELATIVE_LAYOUT;
            view.grid_w = 2;
            view.grid_h = 2;
            view.relative_rect.set = true;
            view.relative_rect.value.set(x, y, 1, 1);
            views.push_back(view);
        }
    }
}

bool buildPreset(Options& options, std::string& error)
{
    if (!options.views.empty()) {
        if (options.scenario != "custom") {
            error = "--view requires --scenario custom";
            return false;
        }
        for (size_t i = 0; i < options.views.size(); ++i) {
            if (options.views[i].group.empty()) {
                std::ostringstream name;
                name << "view" << i;
                options.views[i].group = name.str();
            }
        }
        return true;
    }

    if (options.scenario == "custom") {
        error = "custom scenario requires at least one --view";
        return false;
    }
    if (options.scenario == "single-window") {
        ViewConfig view = presetView(options, "main", 0, 0xff);
        view.mode = DrmDisplayPlane::SINGLE_WINDOW_DISPLAY;
        options.views.push_back(view);
    } else if (options.scenario == "multi-window") {
        appendMultiWindowPreset(options, options.views);
    } else if (options.scenario == "multi-plane") {
        ViewConfig base = presetView(options, "base", 0, 0);
        base.mode = DrmDisplayPlane::SINGLE_WINDOW_DISPLAY;
        options.views.push_back(base);
        ViewConfig overlay = presetView(options, "overlay", 0, 1);
        overlay.plane = selectValue(options.planes, 1, 0);
        overlay.zpos = selectValue(options.zposes, 1, 1);
        overlay.format = V4L2_PIX_FMT_RGB32;
        overlay.mode = DrmDisplayPlane::MULTI_WINDOW_DISPLAY;
        overlay.auto_plane_rect = AUTO_RECT_CENTER_HALF;
        options.views.push_back(overlay);
    } else if (options.scenario == "multi-device") {
        if (options.connectors.size() < 2) {
            error = "multi-device requires at least two --connector IDs";
            return false;
        }
        for (size_t i = 0; i < options.connectors.size(); ++i) {
            std::ostringstream group;
            group << "device" << i;
            ViewConfig view = presetView(
                options, group.str(), i, static_cast<uint32_t>(i));
            view.plane_type = PLANE_TYPE_PRIMARY;
            view.mode = DrmDisplayPlane::SINGLE_WINDOW_DISPLAY;
            options.views.push_back(view);
        }
    } else if (options.scenario == "crop") {
        ViewConfig view = presetView(options, "main", 0, 0xff);
        view.mode = DrmDisplayPlane::SINGLE_WINDOW_DISPLAY;
        view.auto_image_rect = AUTO_RECT_CENTER_HALF;
        options.views.push_back(view);
    } else if (options.scenario == "regions") {
        ViewConfig view = presetView(options, "main", 0, 0xff);
        view.auto_plane_rect = AUTO_RECT_CENTER_THREE_QUARTERS;
        view.auto_window_rect = AUTO_RECT_CENTER_HALF;
        view.auto_image_rect = AUTO_RECT_CENTER_HALF;
        options.views.push_back(view);
    } else if (options.scenario == "all") {
        appendMultiWindowPreset(options, options.views);
        ViewConfig overlay = presetView(options, "overlay", 0, 1);
        overlay.plane = selectValue(options.planes, 1, 0);
        overlay.zpos = selectValue(options.zposes, 1, 1);
        overlay.format = V4L2_PIX_FMT_RGB32;
        overlay.mode = DrmDisplayPlane::MULTI_WINDOW_DISPLAY;
        overlay.auto_plane_rect = AUTO_RECT_CENTER_HALF;
        overlay.auto_image_rect = AUTO_RECT_CENTER_HALF;
        options.views.push_back(overlay);
        if (options.connectors.size() >= 2) {
            ViewConfig second = presetView(options, "device1", 1, 1);
            second.plane = selectValue(options.planes, 2, 0);
            second.zpos = selectValue(options.zposes, 2, 1);
            second.plane_type = PLANE_TYPE_PRIMARY;
            second.mode = DrmDisplayPlane::SINGLE_WINDOW_DISPLAY;
            options.views.push_back(second);
        }
    } else {
        error = "unknown scenario: " + options.scenario;
        return false;
    }
    return true;
}

bool parseOptions(int argc, char** argv, Options& options, std::string& error)
{
    static const option long_options[] = {
        {"scenario", required_argument, nullptr, 's'},
        {"view", required_argument, nullptr, 'V'},
        {"connector", required_argument, nullptr, 'C'},
        {"plane", required_argument, nullptr, 'p'},
        {"zpos", required_argument, nullptr, 'z'},
        {"loop", no_argument, nullptr, 'l'},
        {"duration", required_argument, nullptr, 'd'},
        {"frames", required_argument, nullptr, 'f'},
        {"decoder-buffers", required_argument, nullptr, 'b'},
        {"dry-run", no_argument, nullptr, 1000},
        {"decoder-afbc", no_argument, nullptr, 1001},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0},
    };

    int option_index = 0;
    int value = 0;
    while ((value = getopt_long(argc, argv, "s:V:C:p:z:ld:f:b:h",
                                long_options, &option_index))
           != -1) {
        uint64_t number = 0;
        switch (value) {
            case 's':
                options.scenario = optarg;
                break;
            case 'V': {
                ViewConfig view;
                if (!parseView(optarg, view, error))
                    return false;
                options.views.push_back(view);
                break;
            }
            case 'C':
            case 'p':
            case 'z': {
                uint32_t parsed = 0;
                if (!parseUint32(optarg, parsed)) {
                    error = std::string("invalid numeric option: ") + optarg;
                    return false;
                }
                if (value == 'C')
                    options.connectors.push_back(parsed);
                else if (value == 'p')
                    options.planes.push_back(parsed);
                else
                    options.zposes.push_back(parsed);
                break;
            }
            case 'l':
                options.loop = true;
                break;
            case 'd':
                if (!parseUnsigned(optarg, number) || number > UINT32_MAX) {
                    error = "invalid duration";
                    return false;
                }
                options.duration_seconds = static_cast<unsigned>(number);
                break;
            case 'f':
                if (!parseUnsigned(optarg, options.frame_limit)) {
                    error = "invalid frame limit";
                    return false;
                }
                break;
            case 'b':
                if (!parseUnsigned(optarg, number) || number == 0
                    || number > UINT16_MAX) {
                    error = "invalid decoder buffer count";
                    return false;
                }
                options.decoder_buffers = static_cast<unsigned>(number);
                break;
            case 1000:
                options.dry_run = true;
                break;
            case 1001:
                options.decoder_afbc = true;
                break;
            case 'h':
                printUsage(argv[0]);
                exit(0);
            default:
                error = "invalid command line";
                return false;
        }
    }

    if (optind + 1 != argc) {
        error = "exactly one input FILE is required";
        return false;
    }
    options.input = argv[optind];
    if (!buildPreset(options, error))
        return false;
    return true;
}

bool sameOptionalRect(const OptionalRect& left, const OptionalRect& right)
{
    if (left.set != right.set)
        return false;
    if (!left.set)
        return true;
    Rect a = left.value;
    Rect b = right.value;
    return a == b;
}

bool groupCompatible(const ViewConfig& left, const ViewConfig& right)
{
    return left.screen == right.screen && left.connector == right.connector
           && left.plane == right.plane && left.plane_type == right.plane_type
           && left.zpos == right.zpos && left.linear == right.linear
           && left.format == right.format
           && left.mode == right.mode
           && left.layout == right.layout && left.grid_w == right.grid_w
           && left.grid_h == right.grid_h
           && sameOptionalRect(left.plane_rect, right.plane_rect)
           && left.auto_plane_rect == right.auto_plane_rect;
}

Rect centeredRect(uint32_t parent_width, uint32_t parent_height,
                  uint32_t numerator, uint32_t denominator)
{
    uint32_t width = std::max<uint32_t>(4, parent_width * numerator / denominator);
    uint32_t height = std::max<uint32_t>(4, parent_height * numerator / denominator);
    width &= ~3U;
    height &= ~3U;
    return Rect((parent_width - width) / 2, (parent_height - height) / 2,
                width, height);
}

OptionalRect resolveAutoRect(const OptionalRect& explicit_rect,
                             AutoRect automatic, uint32_t parent_width,
                             uint32_t parent_height)
{
    if (explicit_rect.set)
        return explicit_rect;
    OptionalRect result;
    if (automatic == AUTO_RECT_NONE)
        return result;
    result.set = true;
    if (automatic == AUTO_RECT_FULL)
        result.value.set(0, 0, parent_width, parent_height);
    else if (automatic == AUTO_RECT_CENTER_HALF)
        result.value = centeredRect(parent_width, parent_height, 1, 2);
    else
        result.value = centeredRect(parent_width, parent_height, 3, 4);
    return result;
}

const char* modeName(DrmDisplayPlane::PLANE_DISPLAY_MODE mode)
{
    return mode == DrmDisplayPlane::SINGLE_WINDOW_DISPLAY ? "single" : "multi";
}

void printRect(const char* name, const OptionalRect& rect)
{
    if (!rect.set)
        return;
    std::cout << ' ' << name << '=' << rect.value.x << ':' << rect.value.y
              << ':' << rect.value.w << ':' << rect.value.h;
}

void printConfiguration(const Options& options)
{
    std::cout << "ModuleDrmDisplay full-function test: scenario="
              << options.scenario
              << " input=" << options.input << " loop=" << options.loop
              << " decoder_afbc=" << options.decoder_afbc
              << " duration=" << options.duration_seconds
              << " frame_limit=" << options.frame_limit << '\n';
    for (size_t i = 0; i < options.views.size(); ++i) {
        const ViewConfig& view = options.views[i];
        std::cout << "view[" << i << "] group=" << view.group
                  << " screen=" << view.screen
                  << " connector=" << view.connector
                  << " plane=" << view.plane << " zpos=" << view.zpos
                  << " format=" << v4l2GetFmtName(view.format)
                  << " mode=" << modeName(view.mode)
                  << " layout="
                  << (view.layout == DrmDisplayPlane::RELATIVE_LAYOUT
                          ? "relative"
                          : "absolute");
        printRect("plane", view.plane_rect);
        printRect("window", view.window_rect);
        printRect("relative", view.relative_rect);
        printRect("image", view.image_rect);
        std::cout << '\n';
    }
}

bool validateViews(const Options& options, std::string& error)
{
    std::map<std::string, ViewConfig> groups;
    std::map<std::string, size_t> counts;
    for (size_t i = 0; i < options.views.size(); ++i) {
        const ViewConfig& view = options.views[i];
        if (view.group.empty()) {
            error = "view group cannot be empty";
            return false;
        }
        if (view.layout == DrmDisplayPlane::RELATIVE_LAYOUT
            && !view.relative_rect.set) {
            error = "relative layout requires relative-rect";
            return false;
        }
        std::map<std::string, ViewConfig>::iterator found = groups.find(view.group);
        if (found == groups.end())
            groups[view.group] = view;
        else if (!groupCompatible(found->second, view)) {
            error = "views in group '" + view.group
                    + "' have inconsistent plane settings";
            return false;
        }
        ++counts[view.group];
    }
    for (std::map<std::string, ViewConfig>::const_iterator it = groups.begin();
         it != groups.end(); ++it) {
        if (it->second.mode == DrmDisplayPlane::SINGLE_WINDOW_DISPLAY
            && counts[it->first] != 1) {
            error = "single display mode requires exactly one view in group '"
                    + it->first + "'";
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char** argv)
{
    Options options;
    std::string error;
    if (!parseOptions(argc, argv, options, error)
        || !validateViews(options, error)) {
        std::cerr << "error: " << error << "\n\n";
        printUsage(argv[0]);
        return 2;
    }

    printConfiguration(options);
    if (options.dry_run)
        return 0;

    struct stat input_stat;
    if (stat(options.input.c_str(), &input_stat) != 0
        || !S_ISREG(input_stat.st_mode)) {
        std::cerr << "error: input is not a readable regular file: "
                  << options.input << '\n';
        return 2;
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::atomic_bool failed(false);
    std::atomic_bool source_eos(false);
    std::atomic_bool decoded_afbc_seen(false);
    std::atomic<uint64_t> decoded_frames(0);

    std::shared_ptr<ModuleFileReader> reader = std::make_shared<ModuleFileReader>(options.input, options.loop);
    reader->setBufferCount(20);
    reader->setMediaStatusChangeHooker(
        [&failed, &source_eos](const std::string& name, MediaStatus status) {
            std::cout << name << " status=" << static_cast<int>(status) << '\n';
            if (status == MediaStatus::EOS)
                source_eos = true;
            else if (status == MediaStatus::ABNORMAL) {
                failed = true;
                stop_requested = true;
            }
        });
    int result = reader->init();
    if (result < 0) {
        std::cerr << "ModuleFileReader init failed: " << result << '\n';
        return 1;
    }

    std::shared_ptr<ModuleMppDec> decoder = std::make_shared<ModuleMppDec>();
    result = decoder->connectProducer(reader);
    if (result < 0) {
        std::cerr << "ModuleMppDec connect failed: " << result << '\n';
        return 1;
    }
    decoder->setBufferCount(static_cast<uint16_t>(options.decoder_buffers));
    if (options.decoder_afbc) {
        ImagePara decoder_output = decoder->getOutputImagePara();
        decoder_output.compression = ImageCompression::Afbc16x16;
        decoder->setOutputImagePara(decoder_output);
    }
    decoder->setMediaBufferProduceHooker(
        [&decoded_frames, &decoded_afbc_seen, &options](
            const std::string&, int, std::shared_ptr<MediaBuffer> buffer) {
            if (buffer && buffer->getImagePara().compression == ImageCompression::Afbc16x16) {
                decoded_afbc_seen = true;
            }
            const uint64_t count = ++decoded_frames;
            if (count == 1 || count % 100 == 0)
                std::cout << "decoded_frames=" << count << '\n';
            if (options.frame_limit != 0 && count >= options.frame_limit)
                stop_requested = true;
        });
    decoder->setMediaStatusChangeHooker(
        [&failed](const std::string& name, MediaStatus status) {
            std::cout << name << " status=" << static_cast<int>(status) << '\n';
            if (status == MediaStatus::ABNORMAL) {
                failed = true;
                stop_requested = true;
            }
        });
    result = decoder->init();
    if (result < 0) {
        std::cerr << "ModuleMppDec init failed: " << result << '\n';
        return 1;
    }

    const ImagePara decoded_para = decoder->getOutputImagePara();
    for (size_t i = 0; i < options.views.size(); ++i) {
        options.views[i].image_rect = resolveAutoRect(
            options.views[i].image_rect, options.views[i].auto_image_rect,
            decoded_para.width, decoded_para.height);
    }

    std::map<std::string, size_t> group_indexes;
    std::vector<GroupRuntime> groups;
    for (size_t i = 0; i < options.views.size(); ++i) {
        const ViewConfig& view = options.views[i];
        std::map<std::string, size_t>::iterator found = group_indexes.find(view.group);
        if (found != group_indexes.end()) {
            ++groups[found->second].window_count;
            continue;
        }

        GroupRuntime group;
        group.config = view;
        group.window_count = 1;
        group.plane = std::make_shared<DrmDisplayPlane>(
            view.format, view.screen, view.zpos);
        if (group.plane->setPlanePara(
                view.format, view.plane, view.plane_type, view.zpos,
                view.linear, view.connector)
            < 0) {
            std::cerr << "Failed to set plane parameters for group " << view.group
                      << '\n';
            return 1;
        }
        group.plane->setWindowLayoutMode(view.layout);
        if (view.layout == DrmDisplayPlane::RELATIVE_LAYOUT
            && !group.plane->splitPlane(view.grid_w, view.grid_h)) {
            std::cerr << "Failed to split plane for group " << view.group
                      << '\n';
            return 1;
        }

        const size_t index = groups.size();
        group_indexes[view.group] = index;
        groups.push_back(group);
    }

    for (size_t i = 0; i < groups.size(); ++i) {
        GroupRuntime& group = groups[i];
        uint32_t screen_width = 0;
        uint32_t screen_height = 0;
        group.plane->getScreenResolution(&screen_width, &screen_height);
        if (screen_width == 0 || screen_height == 0) {
            std::cerr << "Failed to query screen resolution for group "
                      << group.config.group << '\n';
            return 1;
        }
        group.config.plane_rect = resolveAutoRect(
            group.config.plane_rect, group.config.auto_plane_rect,
            screen_width, screen_height);
        if (group.config.plane_rect.set) {
            const Rect& rect = group.config.plane_rect.value;
            if (!group.plane->setRect(rect.x, rect.y, rect.w, rect.h)) {
                std::cerr << "Failed to set plane rect for group "
                          << group.config.group << '\n';
                return 1;
            }
        }
    }

    std::vector<std::shared_ptr<ModuleDrmDisplay>> displays;
    std::vector<std::shared_ptr<std::atomic<uint64_t>>> display_frames;
    for (size_t i = 0; i < options.views.size(); ++i) {
        ViewConfig view = options.views[i];
        GroupRuntime& group = groups[group_indexes[view.group]];
        const Rect& plane_rect = group.config.plane_rect.value;
        view.window_rect = resolveAutoRect(
            view.window_rect, view.auto_window_rect, plane_rect.w, plane_rect.h);

        std::shared_ptr<ModuleDrmDisplay> display = std::make_shared<ModuleDrmDisplay>(decoded_para, group.plane);
        display->setSynchronize(
            std::make_shared<Synchronize>(SYNCHRONIZETYPE_VIDEO));
        display->setPlanePara(view.format, view.plane, view.plane_type,
                              view.zpos, view.linear, view.connector);
        display->setBufferCount(1);
        if (display->setPlaneDisplayMode(view.mode) < 0) {
            std::cerr << "Display " << i << " cannot enter "
                      << modeName(view.mode) << "-window mode\n";
            return 1;
        }
        if (view.layout == DrmDisplayPlane::RELATIVE_LAYOUT) {
            const Rect& rect = view.relative_rect.value;
            if (!display->setWindowRelativeRect(rect.x, rect.y, rect.w, rect.h,
                                                false)) {
                std::cerr << "Failed to set relative window rect for view " << i
                          << '\n';
                return 1;
            }
        } else if (view.window_rect.set) {
            const Rect& rect = view.window_rect.value;
            if (!display->setWindowRect(rect.x, rect.y, rect.w, rect.h)) {
                std::cerr << "Failed to set window rect for view " << i << '\n';
                return 1;
            }
        }
        if (view.image_rect.set) {
            const Rect& rect = view.image_rect.value;
            if (!display->setImageRect(rect.x, rect.y, rect.w, rect.h)) {
                std::cerr << "Failed to set image rect for view " << i << '\n';
                return 1;
            }
        }
        result = display->connectProducer(decoder);
        if (result < 0) {
            std::cerr << "Display " << i << " connect failed: " << result
                      << '\n';
            return 1;
        }
        result = display->init();
        if (result < 0) {
            std::cerr << "Display " << i << " init failed: " << result << '\n';
            return 1;
        }
        if (!view.visible && !display->setWindowVisibility(false)) {
            std::cerr << "Display " << i << " visibility update failed\n";
            return 1;
        }

        std::shared_ptr<std::atomic<uint64_t>> count = std::make_shared<std::atomic<uint64_t>>(0);
        display->setMediaBufferConsumeHooker(
            [count](const std::string&, int, std::shared_ptr<MediaBuffer>) {
                ++(*count);
            });
        display->setMediaStatusChangeHooker(
            [&failed, i](const std::string& name, MediaStatus status) {
                std::cout << "display[" << i << "] " << name
                          << " status=" << static_cast<int>(status) << '\n';
                if (status == MediaStatus::ABNORMAL) {
                    failed = true;
                    stop_requested = true;
                }
            });
        displays.push_back(display);
        display_frames.push_back(count);
    }

    std::cout << "Starting display test PID=" << getpid() << " views="
              << displays.size() << " decoded=" << decoded_para.width << 'x'
              << decoded_para.height << ' '
              << v4l2GetFmtName(decoded_para.v4l2Fmt) << '\n';
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    reader->start();

    bool eos_draining = false;
    std::chrono::steady_clock::time_point eos_time;
    while (!stop_requested) {
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        const uint64_t elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
        if (options.duration_seconds != 0
            && elapsed >= options.duration_seconds)
            break;
        if (!options.loop && source_eos) {
            if (!eos_draining) {
                eos_draining = true;
                eos_time = now;
            } else if (std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - eos_time)
                           .count()
                       >= 750) {
                break;
            }
        }
        usleep(100000);
    }

    reader->stop();
    uint64_t total_display_frames = 0;
    for (size_t i = 0; i < display_frames.size(); ++i) {
        const uint64_t count = display_frames[i]->load();
        total_display_frames += count;
        std::cout << "display[" << i << "] input_frames=" << count << '\n';
        if (count == 0)
            failed = true;
    }
    std::cout << "decoded_frames=" << decoded_frames.load()
              << " total_display_inputs=" << total_display_frames << '\n';

    for (size_t i = 0; i < displays.size(); ++i)
        displays[i]->removeProductor(decoder);
    displays.clear();
    groups.clear();
    if (decoded_frames == 0) {
        std::cerr << "No decoded frames were produced\n";
        failed = true;
    }
    if (options.decoder_afbc && !decoded_afbc_seen) {
        std::cerr << "Decoder did not produce AFBC input frames\n";
        failed = true;
    }
    if (failed) {
        std::cerr << "ModuleDrmDisplay full-function test failed\n";
        return 1;
    }
    std::cout << "ModuleDrmDisplay full-function test completed successfully\n";
    return 0;
}

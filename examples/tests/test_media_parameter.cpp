// Full-use test for the path-based MediaParameter API.
#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "module/ff_media_parameter_helpers.hpp"
#include "module/module_media.hpp"
#if OPENGL_SUPPORT
#include "module/vp/module_imageProcessor.hpp"
#endif

using namespace FFMedia;

namespace
{

class ConfigurableModule : public ModuleMedia
{
public:
    ConfigurableModule()
        : ModuleMedia("configurable-module"), bitrate_(1000), mode_(0),
          duplicate_getter_calls_(0),
          crop_{0, 0, 1920, 1080, {0, 0, 640, 360, {0, 0}}}
    {
        ParameterInfo bitrate = integerParameter(
            "bitrate", 1000, 100, 10000, "Target bitrate", "bit/s");
        ParameterInfo low_latency = booleanParameter(
            "low-latency", false, "Enable low-latency mode");
        low_latency.flags |= PARAMETER_FLAG_RUNTIME;

        ParameterInfo mode = integerParameter("mode", 0, 0, 2);
        mode.enum_values = {
            ParameterEnumValue("fast", 0, "Prefer throughput"),
            ParameterEnumValue("quality", 1, "Prefer quality"),
        };

        ParameterInfo crop = objectParameter(
            "crop",
            {
                integerParameter("x", 0, 0, 8192),
                integerParameter("y", 0, 0, 8192),
                integerParameter("width", 1920, 1, 8192),
                integerParameter("height", 1080, 1, 8192),
                objectParameter(
                    "region",
                    {
                        integerParameter("x", 0, 0, 8192),
                        integerParameter("y", 0, 0, 8192),
                        integerParameter("width", 640, 1, 8192),
                        integerParameter("height", 360, 1, 8192),
                        objectParameter(
                            "offset",
                            {
                                integerParameter("x", 0, -8192, 8192),
                                integerParameter("y", 0, -8192, 8192),
                            },
                            "Nested offset"),
                    },
                    "Nested region"),
            },
            "Visible image rectangle",
            PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE, true);

        static const auto schema = makeParameterSchema({
            bitrate,
            low_latency,
            stringParameter("preset", "balanced"),
            mode,
            crop,
        });

        assert(installParameterSchema(
                   schema,
                   {
                       bindParameter(
                           "bitrate", [this]() { return bitrate_; },
                           [this](int64_t value) {
                               bitrate_ = value;
                               return 0;
                           }),
                       bindParameter(
                           "mode", [this]() { return mode_; },
                           [this](int64_t value) {
                               mode_ = value;
                               return 0;
                           }),
                   },
                   {bindParameterObject(
                       "crop",
                       [this](ParameterObject& value) {
                           value = cropObject();
                           return 0;
                       },
                       [this](const ParameterObject& value) {
                           Crop next{};
                           if (!objectToCrop(value, next))
                               return -EINVAL;
                           if (next.x + next.width > 1920
                               || next.y + next.height > 1080
                               || next.region.x + next.region.width
                                      > next.width
                               || next.region.y + next.region.height
                                      > next.height) {
                               return -ERANGE;
                           }
                           crop_ = next;
                           return 0;
                       })})
               == 0);
    }

    int64_t bitrate() const { return bitrate_; }
    int64_t mode() const { return mode_; }

    int registerDuplicate()
    {
        static const auto duplicate = makeParameterSchema({stringParameter("preset", "")});
        return installParameterSchema(
            duplicate,
            {bindParameter(
                "preset",
                [this]() {
                    ++duplicate_getter_calls_;
                    return std::string("balanced");
                },
                [](const std::string&) {})});
    }
    int duplicateGetterCalls() const { return duplicate_getter_calls_; }

private:
    struct Offset {
        int64_t x;
        int64_t y;
    };
    struct Region {
        int64_t x;
        int64_t y;
        int64_t width;
        int64_t height;
        Offset offset;
    };
    struct Crop {
        int64_t x;
        int64_t y;
        int64_t width;
        int64_t height;
        Region region;
    };

    ParameterObject cropObject() const
    {
        return ParameterObject(
            {{"x", crop_.x},
             {"y", crop_.y},
             {"width", crop_.width},
             {"height", crop_.height},
             {"region",
              {{"x", crop_.region.x},
               {"y", crop_.region.y},
               {"width", crop_.region.width},
               {"height", crop_.region.height},
               {"offset",
                {{"x", crop_.region.offset.x},
                 {"y", crop_.region.offset.y}}}}}});
    }

    static bool objectToCrop(const ParameterObject& value, Crop& crop)
    {
        ParameterObject region;
        ParameterObject offset;
        return value.getMember("x", crop.x) == 0
               && value.getMember("y", crop.y) == 0
               && value.getMember("width", crop.width) == 0
               && value.getMember("height", crop.height) == 0
               && value.getMember("region", region) == 0
               && region.getMember("x", crop.region.x) == 0
               && region.getMember("y", crop.region.y) == 0
               && region.getMember("width", crop.region.width) == 0
               && region.getMember("height", crop.region.height) == 0
               && region.getMember("offset", offset) == 0
               && offset.getMember("x", crop.region.offset.x) == 0
               && offset.getMember("y", crop.region.offset.y) == 0;
    }

    int64_t bitrate_;
    int64_t mode_;
    int duplicate_getter_calls_;
    Crop crop_;
};

struct ExternalParameterData {
    int64_t count;
};

class TableModule : public MediaParameter
{
public:
    enum class Mode {
        FAST = 0,
        QUALITY = 1,
    };

    TableModule(ExternalParameterData& external, int64_t& external_value)
        : impl_(new ImplData{6}), enabled_(false), mode_(Mode::FAST),
          quality_(0.8f), device_("default"), narrow_(0), custom_value_(3)
    {
        ParameterInfo enabled = booleanParameter(
            "enabled", false, "Enable processing",
            PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE
                | PARAMETER_FLAG_RUNTIME);
        ParameterInfo mode = integerParameter("mode", 0, 0, 2);
        mode.enum_values = {
            ParameterEnumValue("fast", 0),
            ParameterEnumValue("quality", 1),
        };

        static const auto schema = makeParameterSchema({
            integerParameter("count", 4, 0, 100),
            enabled,
            mode,
            doubleParameter("quality", 0.8, 0.0, 1.0),
            stringParameter("device", "default"),
            integerParameter("external-count", 0, 0, 100),
            integerParameter("external-value", 0, 0, 100),
            integerParameter("narrow", 0, 0, 1000),
            integerParameter("custom-value", 3, 0, 100),
        });

        assert(installParameterSchema(
                   schema,
                   {
                       bindParameter("count", *impl_, &ImplData::count),
                       bindParameter("enabled", *this,
                                     &TableModule::enabled_),
                       bindParameter("mode", *this, &TableModule::mode_),
                       bindParameter("quality", *this,
                                     &TableModule::quality_),
                       bindParameter("device", *this,
                                     &TableModule::device_),
                       bindParameter("external-count", external,
                                     &ExternalParameterData::count),
                       bindParameter("external-value", external_value),
                       bindParameter("narrow", narrow_),
                       bindParameter(
                           "custom-value",
                           [this]() { return custom_value_; },
                           [this](int64_t value) {
                               if (value == 13)
                                   return -EPERM;
                               custom_value_ = value;
                               return 0;
                           }),
                   })
               == 0);
    }

    uint32_t count() const { return impl_->count; }
    bool enabled() const { return enabled_; }
    Mode mode() const { return mode_; }
    float quality() const { return quality_; }
    const std::string& device() const { return device_; }
    uint8_t narrow() const { return narrow_; }
    int64_t customValue() const { return custom_value_; }

private:
    struct ImplData {
        uint32_t count;
    };

    std::unique_ptr<ImplData> impl_;
    bool enabled_;
    Mode mode_;
    float quality_;
    std::string device_;
    uint8_t narrow_;
    int64_t custom_value_;
};

class StoredObjectModule : public MediaParameter
{
public:
    StoredObjectModule()
    {
        static const auto schema = makeParameterSchema({
            objectParameter(
                "stored",
                {integerParameter("value", 7, 0, 100),
                 stringParameter("name", "default")},
                "Owned atomic object",
                PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE, true),
        });
        assert(installParameterSchema(schema) == 0);
    }
};

class StatefulModule : public MediaParameter
{
public:
    StatefulModule()
        : state_(1), value_(2)
    {
        ParameterInfo value = integerParameter("value", 2, 0, 10);
        value.writable_states = 1;
        value.apply_mode = ParameterApplyMode::NEXT_START;
        static const auto schema = makeParameterSchema({value});
        setParameterStateGetter([this]() { return state_; });
        assert(installParameterSchema(
                   schema, {bindParameter("value", value_)})
               == 0);
    }

    void setState(ParameterStateMask state) { state_ = state; }

private:
    ParameterStateMask state_;
    int64_t value_;
};

class InvalidAtomicBindingModule : public MediaParameter
{
public:
    InvalidAtomicBindingModule()
        : value_(1) {}

    int installInvalidBinding()
    {
        static const auto schema = makeParameterSchema({
            objectParameter(
                "atomic", {integerParameter("value", 1, 0, 10)}, "",
                PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE, true),
        });
        return installParameterSchema(
            schema, {bindParameter("atomic/value", value_)});
    }

private:
    int64_t value_;
};

class InvalidSchemaModule : public MediaParameter
{
public:
    int installOutOfRangeEnum()
    {
        ParameterInfo value = integerParameter("enum", 0, 0, 1);
        value.enum_values = {
            ParameterEnumValue("zero", 0),
            ParameterEnumValue("outside", 2),
        };
        return installParameterSchema(makeParameterSchema({value}));
    }

    int installNonFiniteBoundary()
    {
        ParameterInfo value = doubleParameter("double", 0.0, 0.0, 1.0);
        value.minimum = ParameterValue(
            std::numeric_limits<double>::infinity());
        return installParameterSchema(makeParameterSchema({value}));
    }
};

class MetadataInheritanceModule : public MediaParameter
{
public:
    MetadataInheritanceModule()
    {
        ParameterInfo value = integerParameter("value", 1, 0, 10);
        value.writable_states = 0x3;

        ParameterInfo group = objectParameter("group", {value});
        group.flags |= PARAMETER_FLAG_RUNTIME;
        group.writable_states = 0x6;
        group.apply_mode = ParameterApplyMode::RECONFIGURE;

        ParameterInfo locked_value = integerParameter("value", 2, 0, 10);
        ParameterInfo locked = objectParameter(
            "locked", {locked_value}, "Read-only inherited object",
            PARAMETER_FLAG_READABLE | PARAMETER_FLAG_RUNTIME);

        ParameterInfo root = objectParameter("metadata", {group, locked});
        root.flags |= PARAMETER_FLAG_DEPRECATED;
        root.writable_states = 0xe;
        root.apply_mode = ParameterApplyMode::NEXT_START;

        static const auto schema = makeParameterSchema({root});
        assert(installParameterSchema(schema) == 0);
    }
};

class ScalarFastPathModule : public MediaParameter
{
public:
    ScalarFastPathModule()
        : getter_available_(true), value_(1), setter_calls_(0)
    {
        static const auto schema = makeParameterSchema({
            integerParameter("value", 1, 0, 10),
        });

        ParameterBinding binding;
        binding.path = "value";
        binding.getter = [this](ParameterValue& value) {
            if (!getter_available_)
                return -EIO;
            value = ParameterValue(value_);
            return 0;
        };
        binding.setter = [this](const ParameterValue& value) {
            int64_t integer = 0;
            if (!value.getInteger(integer))
                return -EINVAL;
            value_ = integer;
            ++setter_calls_;
            return 0;
        };
        assert(installParameterSchema(schema, {binding}) == 0);
        getter_available_ = false;
    }

    int64_t value() const { return value_; }
    int setterCalls() const { return setter_calls_; }

private:
    bool getter_available_;
    int64_t value_;
    int setter_calls_;
};

class LifecycleParameterModule : public ModuleMedia
{
public:
    LifecycleParameterModule()
        : ModuleMedia("lifecycle-parameter-module"), value_(0),
          setter_entered_(false), release_setter_(false)
    {
        ParameterInfo value = integerParameter("lifecycle-value", 0, 0, 10);
        value.writable_states = static_cast<ParameterStateMask>(1)
                                << static_cast<int>(MediaStatus::CREATED);
        static const auto schema = makeParameterSchema({value});
        assert(installParameterSchema(
                   schema,
                   {bindParameter(
                       "lifecycle-value", [this]() { return value_; },
                       [this](int64_t value) {
                           auto configuration_lock = lockConfiguration();
                           std::unique_lock<std::mutex> lock(test_mutex_);
                           setter_entered_ = true;
                           test_condition_.notify_all();
                           test_condition_.wait(
                               lock,
                               [this]() { return release_setter_; });
                           value_ = value;
                           return 0;
                       })})
               == 0);
    }

    void waitForSetter()
    {
        std::unique_lock<std::mutex> lock(test_mutex_);
        test_condition_.wait(lock, [this]() { return setter_entered_; });
    }

    void releaseSetter()
    {
        std::lock_guard<std::mutex> lock(test_mutex_);
        release_setter_ = true;
        test_condition_.notify_all();
    }

    int64_t value() const { return value_; }

private:
    int64_t value_;
    std::mutex test_mutex_;
    std::condition_variable test_condition_;
    bool setter_entered_;
    bool release_setter_;
};

class NonAtomicCommitModule : public MediaParameter
{
public:
    NonAtomicCommitModule()
        : first_(1), second_(2), child_x_(3), child_y_(4),
          child_set_count_(0)
    {
        static const auto schema = makeParameterSchema({
            objectParameter(
                "root",
                {
                    integerParameter("first", 1, 0, 100),
                    integerParameter("second", 2, 0, 100),
                    integerParameter("local-a", 5, 0, 100),
                    integerParameter("local-b", 6, 0, 100),
                    objectParameter(
                        "child",
                        {
                            integerParameter("x", 3, 0, 100),
                            integerParameter("y", 4, 0, 100),
                        },
                        "Atomic child", PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE,
                        true),
                },
                "Non-atomic parent"),
        });
        assert(installParameterSchema(
                   schema,
                   {
                       bindParameter("root/first", first_),
                       bindParameter(
                           "root/second", [this]() { return second_; },
                           [this](int64_t value) {
                               if (value == 13)
                                   return -EPERM;
                               second_ = value;
                               return 0;
                           }),
                   },
                   {bindParameterObject(
                       "root/child",
                       [this](ParameterObject& value) {
                           value = ParameterObject(
                               {{"x", child_x_}, {"y", child_y_}});
                           return 0;
                       },
                       [this](const ParameterObject& value) {
                           int64_t x = 0;
                           int64_t y = 0;
                           if (value.getMember("x", x) < 0
                               || value.getMember("y", y) < 0) {
                               return -EINVAL;
                           }
                           child_x_ = x;
                           child_y_ = y;
                           ++child_set_count_;
                           return 0;
                       })})
               == 0);
    }

    int64_t first() const { return first_; }
    int64_t second() const { return second_; }
    int64_t childX() const { return child_x_; }
    int64_t childY() const { return child_y_; }
    int childSetCount() const { return child_set_count_; }

private:
    int64_t first_;
    int64_t second_;
    int64_t child_x_;
    int64_t child_y_;
    int child_set_count_;
};

class CallbackModule : public MediaParameter
{
public:
    CallbackModule()
        : source_(3), mirror_(4), nested_write_(0), blocking_(0),
          nested_read_result_(-EINPROGRESS),
          nested_write_result_(-EINPROGRESS), setter_entered_(false),
          release_setter_(false)
    {
        static const auto source_schema = makeParameterSchema({
            integerParameter("source", 3, 0, 100),
        });
        assert(installParameterSchema(
                   source_schema, {bindParameter("source", source_)})
               == 0);

        static const auto callback_schema = makeParameterSchema({
            integerParameter("mirror", 4, 0, 100),
            integerParameter("nested-write", 0, 0, 100),
            integerParameter("blocking", 0, 0, 100),
        });
        assert(installParameterSchema(
                   callback_schema,
                   {
                       bindParameter(
                           "mirror",
                           [this]() {
                               int64_t source = 0;
                               nested_read_result_ = getParameter("source", source);
                               return mirror_;
                           },
                           [this](int64_t value) {
                               int64_t source = 0;
                               nested_read_result_ = getParameter("source", source);
                               if (nested_read_result_ < 0)
                                   return nested_read_result_;
                               mirror_ = value;
                               return 0;
                           }),
                       bindParameter(
                           "nested-write", [this]() { return nested_write_; },
                           [this](int64_t value) {
                               nested_write_result_ = setParameter("source", value);
                               return nested_write_result_;
                           }),
                       bindParameter(
                           "blocking", [this]() { return blocking_; },
                           [this](int64_t value) {
                               std::unique_lock<std::mutex> lock(test_mutex_);
                               setter_entered_ = true;
                               test_condition_.notify_all();
                               test_condition_.wait(
                                   lock,
                                   [this]() { return release_setter_; });
                               blocking_ = value;
                               return 0;
                           }),
                   })
               == 0);
    }

    void waitForSetter()
    {
        std::unique_lock<std::mutex> lock(test_mutex_);
        test_condition_.wait(lock, [this]() { return setter_entered_; });
    }

    void releaseSetter()
    {
        std::lock_guard<std::mutex> lock(test_mutex_);
        release_setter_ = true;
        test_condition_.notify_all();
    }

    int nestedReadResult() const { return nested_read_result_; }
    int nestedWriteResult() const { return nested_write_result_; }

private:
    int64_t source_;
    int64_t mirror_;
    int64_t nested_write_;
    int64_t blocking_;
    int nested_read_result_;
    int nested_write_result_;
    std::mutex test_mutex_;
    std::condition_variable test_condition_;
    bool setter_entered_;
    bool release_setter_;
};

class LoopGetterModule : public MediaParameter
{
public:
    LoopGetterModule()
        : loop_enabled_(false)
    {
        ParameterInfo first = integerParameter("first", 1, 0, 100);
        first.flags = PARAMETER_FLAG_READABLE;
        ParameterInfo second = integerParameter("second", 2, 0, 100);
        second.flags = PARAMETER_FLAG_READABLE;
        static const auto schema = makeParameterSchema({first, second});

        ParameterBinding first_binding;
        first_binding.path = "first";
        first_binding.getter = [this](ParameterValue& value) {
            if (loop_enabled_) {
                int64_t nested = 0;
                const int ret = getParameter("second", nested);
                if (ret < 0)
                    return ret;
            }
            value = ParameterValue(1);
            return 0;
        };
        ParameterBinding second_binding;
        second_binding.path = "second";
        second_binding.getter = [this](ParameterValue& value) {
            if (loop_enabled_) {
                int64_t nested = 0;
                const int ret = getParameter("first", nested);
                if (ret < 0)
                    return ret;
            }
            value = ParameterValue(2);
            return 0;
        };
        assert(installParameterSchema(
                   schema, {first_binding, second_binding})
               == 0);
        loop_enabled_ = true;
    }

    void disableLoop() { loop_enabled_ = false; }

private:
    bool loop_enabled_;
};

class MixedAccessObjectModule : public MediaParameter
{
public:
    MixedAccessObjectModule()
        : state_(0x1)
    {
        ParameterInfo read_only = integerParameter("read-only", 2, 0, 100);
        read_only.flags = PARAMETER_FLAG_READABLE;
        ParameterInfo write_only = integerParameter("write-only", 3, 0, 100);
        write_only.flags = PARAMETER_FLAG_WRITABLE;
        ParameterInfo state_limited = integerParameter(
            "state-limited", 4, 0, 100);
        state_limited.writable_states = 0x2;
        ParameterInfo nested_read_only = integerParameter(
            "locked", 5, 0, 100);
        nested_read_only.flags = PARAMETER_FLAG_READABLE;

        static const auto schema = makeParameterSchema({
            objectParameter(
                "mixed",
                {
                    integerParameter("value", 1, 0, 100),
                    read_only,
                    write_only,
                    state_limited,
                    objectParameter("nested", {nested_read_only}),
                },
                "Mixed-access atomic object",
                PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE, true),
        });
        setParameterStateGetter([this]() { return state_; });
        assert(installParameterSchema(schema) == 0);
    }

    void setState(ParameterStateMask state) { state_ = state; }

private:
    ParameterStateMask state_;
};

class IncompleteObjectGetterModule : public MediaParameter
{
public:
    int installIncompleteGetter()
    {
        static const auto schema = makeParameterSchema({
            objectParameter(
                "object",
                {integerParameter("first", 1, 0, 100),
                 integerParameter("second", 2, 0, 100)},
                "Complete getter contract",
                PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE, true),
        });
        return installParameterSchema(
            schema, {},
            {bindParameterObject(
                "object",
                [](ParameterObject& value) {
                    value = ParameterObject({{"first", 1}});
                    return 0;
                },
                [](const ParameterObject&) { return 0; })});
    }
};

}  // namespace

int main()
{
    ConfigurableModule module;

    assert(module.hasParameter("buffer-count"));
    assert(module.hasParameter("bitrate"));
    assert(module.hasParameter("crop"));
    assert(module.hasParameter("crop/region/offset/x"));
    assert(!module.hasParameter("missing"));
    assert(!module.hasParameter("/bitrate"));

    const std::vector<ParameterInfo> roots = module.queryParameters();
    assert(roots.size() >= 9);
    assert(roots[0].name == "status");
    assert(roots[1].name == "buffer-count");
    assert(roots[2].name == "buffer-size");
    assert(roots[3].name == "input-queue-size");

    ParameterInfo info;
    assert(module.queryParameter("buffer-count", info) == 0);
    assert(info.writable_states == PARAMETER_STATE_ANY);
    assert(info.apply_mode == ParameterApplyMode::IMMEDIATE);
    assert(module.queryParameter("buffer-size", info) == 0);
    assert(info.writable_states == PARAMETER_STATE_ANY);
    assert(info.apply_mode == ParameterApplyMode::IMMEDIATE);

    assert(module.queryParameter("bitrate", info) == 0);
    assert(info.type == ParameterType::INTEGER);
    assert(info.unit == "bit/s");
    assert(module.queryParameter("crop", info) == 0);
    assert(info.type == ParameterType::OBJECT && info.atomic);
    assert(module.queryParameters("crop").size() == 5);
    assert(module.queryParameters("crop/region").size() == 5);

    int64_t integer = 0;
    assert(module.getParameter("bitrate", integer) == 0);
    assert(integer == 1000);
    assert(module.setParameter("bitrate", 4096) == 0);
    assert(module.bitrate() == 4096);
    assert(module.setParameter("bitrate", 99) == -ERANGE);
    assert(module.setParameter("bitrate", true) == -EINVAL);
    assert(module.setParameter("missing", 1) == -ENOENT);

    assert(module.setParameter("buffer-count", 8) == 0);
    assert(module.getBufferCount() == 8);
    module.setBufferCount(12);
    assert(module.getParameter("buffer-count", integer) == 0);
    assert(integer == 12);
    assert(module.setParameter("buffer-size", 4096) == 0);
    assert(module.getParameter("buffer-size", integer) == 0);
    assert(integer == 4096);
    module.setBufferSize(8192);
    assert(module.getParameter("buffer-size", integer) == 0);
    assert(integer == 8192);
    assert(module.getBufferSize() == 8192);

    assert(module.setParameter("input-queue-size", 256) == 0);
    assert(module.getInputBufferQueueSize() == 256);
    assert(module.setParameter("input-queue-size", 0) == -ERANGE);
    assert(module.getInputBufferQueueSize() == 256);

    bool enabled = false;
    assert(module.setParameterFromString("low-latency", "on") == 0);
    assert(module.getParameter("low-latency", enabled) == 0 && enabled);
    assert(module.setParameterFromString("low-latency", "invalid")
           == -EINVAL);

    std::string text;
    assert(module.setParameter("preset", "quality") == 0);
    assert(module.getParameterAsString("preset", text) == 0);
    assert(text == "quality");
    assert(module.setParameterFromString("mode", "quality") == 0);
    assert(module.mode() == 1);
    assert(module.getParameterAsString("mode", text) == 0);
    assert(text == "quality");
    assert(module.setParameter("mode", 2) == -EINVAL);
    assert(module.registerDuplicate() == -EEXIST);
    assert(module.duplicateGetterCalls() == 0);

    const uint64_t crop_revision = module.parameterRevision();
    assert(module.setParameter(
               "crop",
               ParameterObject(
                   {{"x", 100}, {"y", 50}, {"width", 1280}, {"height", 720}, {"region", {{"x", 100}, {"y", 20}, {"width", 400}, {"height", 200}}}}))
           == 0);
    assert(module.parameterRevision() == crop_revision + 1);
    assert(module.getParameter("crop/x", integer) == 0 && integer == 100);
    assert(module.getParameter("crop/region/width", integer) == 0
           && integer == 400);

    // A descendant write is merged into the complete atomic object and invokes
    // exactly one object setter.
    assert(module.setParameter("crop/region/offset/x", 10) == 0);
    assert(module.getParameter("crop/region/offset/x", integer) == 0
           && integer == 10);
    assert(module.getParameter("crop/region/width", integer) == 0
           && integer == 400);

    // Partial object patches preserve unspecified current members.
    assert(module.setParameter(
               "crop/region",
               ParameterObject({{"y", 30}, {"offset", {{"y", 12}}}}))
           == 0);
    assert(module.getParameter("crop/region/x", integer) == 0
           && integer == 100);
    assert(module.getParameter("crop/region/y", integer) == 0
           && integer == 30);
    assert(module.getParameter("crop/region/offset/x", integer) == 0
           && integer == 10);
    assert(module.getParameter("crop/region/offset/y", integer) == 0
           && integer == 12);

    // Schema and cross-member validation happen before the atomic commit.
    assert(module.setParameter("crop/region/width", 9000) == -ERANGE);
    assert(module.setParameter("crop/x", 1000) == -ERANGE);
    assert(module.getParameter("crop/x", integer) == 0 && integer == 100);
    assert(module.setParameterFromString("crop/region", "value")
           == -ENOTSUP);

    ExternalParameterData external{7};
    int64_t external_value = 9;
    TableModule table(external, external_value);
    assert(table.setParameter("count", 20) == 0);
    assert(table.count() == 20);
    assert(table.setParameter("count", 101) == -ERANGE);
    assert(table.setParameter("enabled", true) == 0 && table.enabled());
    assert(table.setParameterFromString("mode", "quality") == 0);
    assert(table.mode() == TableModule::Mode::QUALITY);
    assert(table.setParameter("quality", 0.95) == 0);
    assert(table.quality() > 0.94f && table.quality() < 0.96f);
    assert(table.setParameter("device", "video0") == 0);
    assert(table.device() == "video0");
    assert(table.setParameter("external-count", 18) == 0);
    assert(external.count == 18);
    assert(table.setParameter("external-value", 24) == 0);
    assert(external_value == 24);
    assert(table.setParameter("custom-value", 25) == 0);
    assert(table.customValue() == 25);
    assert(table.setParameter("custom-value", 13) == -EPERM);
    assert(table.customValue() == 25);

    StoredObjectModule stored;
    assert(stored.getParameter("stored/value", integer) == 0);
    assert(integer == 7);
    assert(stored.setParameter("stored/value", 9) == 0);
    assert(stored.getParameter("stored/value", integer) == 0);
    assert(integer == 9);

    StatefulModule stateful;
    const uint64_t state_revision = stateful.parameterRevision();
    assert(stateful.setParameter("value", 4) == 0);
    assert(stateful.parameterRevision() == state_revision + 1);
    stateful.setState(2);
    assert(stateful.setParameter("value", 5) == -EBUSY);
    assert(stateful.queryParameter("value", info) == 0);
    assert(info.apply_mode == ParameterApplyMode::NEXT_START);

    InvalidAtomicBindingModule invalid_atomic;
    assert(invalid_atomic.installInvalidBinding() == -ENOTSUP);
    assert(!invalid_atomic.hasParameter("atomic"));

    InvalidSchemaModule invalid_schema;
    assert(invalid_schema.installOutOfRangeEnum() == -ERANGE);
    assert(!invalid_schema.hasParameter("enum"));
    assert(invalid_schema.installNonFiniteBoundary() == -EINVAL);
    assert(!invalid_schema.hasParameter("double"));

    MetadataInheritanceModule metadata;
    assert(metadata.queryParameter("metadata/group/value", info) == 0);
    assert((info.flags & PARAMETER_FLAG_READABLE) != 0);
    assert((info.flags & PARAMETER_FLAG_WRITABLE) != 0);
    assert((info.flags & PARAMETER_FLAG_RUNTIME) != 0);
    assert((info.flags & PARAMETER_FLAG_DEPRECATED) != 0);
    assert(info.writable_states == 0x2);
    assert(info.apply_mode == ParameterApplyMode::NEXT_START);

    const std::vector<ParameterInfo> group_members = metadata.queryParameters("metadata/group");
    assert(group_members.size() == 1);
    assert(group_members[0].flags == info.flags);
    assert(group_members[0].writable_states == info.writable_states);
    assert(group_members[0].apply_mode == info.apply_mode);

    assert(metadata.queryParameter("metadata/locked/value", info) == 0);
    assert((info.flags & PARAMETER_FLAG_READABLE) != 0);
    assert((info.flags & PARAMETER_FLAG_WRITABLE) == 0);
    assert((info.flags & PARAMETER_FLAG_RUNTIME) != 0);
    assert((info.flags & PARAMETER_FLAG_DEPRECATED) != 0);
    assert(info.writable_states == 0xe);
    assert(info.apply_mode == ParameterApplyMode::NEXT_START);

    assert(metadata.queryParameter("metadata", info) == 0);
    assert(info.object_members.size() == 2);
    assert(info.object_members[0].object_members.size() == 1);
    assert(info.object_members[0].object_members[0].writable_states == 0x2);
    assert(info.object_members[0].object_members[0].apply_mode
           == ParameterApplyMode::NEXT_START);

    ScalarFastPathModule scalar_fast_path;
    assert(scalar_fast_path.getParameter("value", integer) == -EIO);
    const uint64_t scalar_revision = scalar_fast_path.parameterRevision();
    assert(scalar_fast_path.setParameter("value", 7) == 0);
    assert(scalar_fast_path.value() == 7);
    assert(scalar_fast_path.setterCalls() == 1);
    assert(scalar_fast_path.parameterRevision() == scalar_revision + 1);
    assert(scalar_fast_path.setParameter("value", 11) == -ERANGE);
    assert(scalar_fast_path.value() == 7);
    assert(scalar_fast_path.setterCalls() == 1);
    assert(scalar_fast_path.parameterRevision() == scalar_revision + 1);

    LifecycleParameterModule lifecycle_parameter;
    std::atomic<int> lifecycle_write_result(-EINPROGRESS);
    std::thread lifecycle_writer([&]() {
        lifecycle_write_result.store(
            lifecycle_parameter.setParameter("lifecycle-value", 5),
            std::memory_order_release);
    });
    lifecycle_parameter.waitForSetter();
    std::atomic<bool> start_entered(false);
    std::atomic<bool> start_completed(false);
    std::thread lifecycle_starter([&]() {
        start_entered.store(true, std::memory_order_release);
        lifecycle_parameter.start();
        start_completed.store(true, std::memory_order_release);
    });
    while (!start_entered.load(std::memory_order_acquire))
        std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(!start_completed.load(std::memory_order_acquire));
    assert(lifecycle_parameter.getModuleStatus() == MediaStatus::CREATED);
    lifecycle_parameter.releaseSetter();
    lifecycle_writer.join();
    lifecycle_starter.join();
    assert(lifecycle_write_result.load(std::memory_order_acquire) == 0);
    assert(lifecycle_parameter.value() == 5);
    assert(start_completed.load(std::memory_order_acquire));
    assert(lifecycle_parameter.getModuleStatus() == MediaStatus::STARTED);
    lifecycle_parameter.stop();

    NonAtomicCommitModule non_atomic;
    const uint64_t non_atomic_revision = non_atomic.parameterRevision();
    assert(non_atomic.setParameter(
               "root", ParameterObject({{"first", 7}, {"second", 8}}))
           == -ENOTSUP);
    assert(non_atomic.first() == 1 && non_atomic.second() == 2);
    assert(non_atomic.parameterRevision() == non_atomic_revision);

    assert(non_atomic.setParameter(
               "root", ParameterObject({{"first", 7}}))
           == 0);
    assert(non_atomic.first() == 7 && non_atomic.second() == 2);
    const uint64_t rejected_revision = non_atomic.parameterRevision();
    assert(non_atomic.setParameter(
               "root", ParameterObject({{"second", 13}}))
           == -EPERM);
    assert(non_atomic.second() == 2);
    assert(non_atomic.parameterRevision() == rejected_revision);

    assert(non_atomic.setParameter(
               "root",
               ParameterObject({{"local-a", 20}, {"local-b", 21}}))
           == 0);
    assert(non_atomic.getParameter("root/local-a", integer) == 0
           && integer == 20);
    assert(non_atomic.getParameter("root/local-b", integer) == 0
           && integer == 21);

    assert(non_atomic.setParameter(
               "root",
               ParameterObject({{"first", 8}, {"local-a", 22}}))
           == 0);
    assert(non_atomic.first() == 8);
    assert(non_atomic.getParameter("root/local-a", integer) == 0
           && integer == 22);

    assert(non_atomic.setParameter(
               "root",
               ParameterObject({{"child", {{"x", 9}}},
                                {"local-b", 23}}))
           == 0);
    assert(non_atomic.childX() == 9 && non_atomic.childY() == 4);
    assert(non_atomic.childSetCount() == 1);
    assert(non_atomic.getParameter("root/local-b", integer) == 0
           && integer == 23);
    const uint64_t child_revision = non_atomic.parameterRevision();
    assert(non_atomic.setParameter(
               "root",
               ParameterObject({{"first", 10}, {"child", {{"y", 12}}}}))
           == -ENOTSUP);
    assert(non_atomic.first() == 8);
    assert(non_atomic.childX() == 9 && non_atomic.childY() == 4);
    assert(non_atomic.childSetCount() == 1);
    assert(non_atomic.parameterRevision() == child_revision);

    CallbackModule callbacks;
    assert(callbacks.getParameter("mirror", integer) == 0 && integer == 4);
    assert(callbacks.nestedReadResult() == 0);
    assert(callbacks.setParameter("mirror", 9) == 0);
    assert(callbacks.nestedReadResult() == 0);
    const uint64_t callback_revision = callbacks.parameterRevision();
    assert(callbacks.setParameter("nested-write", 11) == -EDEADLK);
    assert(callbacks.nestedWriteResult() == -EDEADLK);
    assert(callbacks.parameterRevision() == callback_revision);
    assert(callbacks.getParameter("source", integer) == 0 && integer == 3);

    std::atomic<int> blocking_result(-EINPROGRESS);
    std::thread blocking_writer([&]() {
        blocking_result.store(
            callbacks.setParameter("blocking", 12),
            std::memory_order_release);
    });
    callbacks.waitForSetter();
    assert(callbacks.queryParameter("source", info) == 0);
    assert(callbacks.getParameter("source", integer) == -EAGAIN);
    assert(callbacks.setParameter("source", 8) == -EAGAIN);
    callbacks.releaseSetter();
    blocking_writer.join();
    assert(blocking_result.load(std::memory_order_acquire) == 0);
    assert(callbacks.getParameter("blocking", integer) == 0
           && integer == 12);

    LoopGetterModule loop_getter;
    assert(loop_getter.getParameter("first", integer) == -ELOOP);
    loop_getter.disableLoop();
    assert(loop_getter.getParameter("first", integer) == 0
           && integer == 1);

    MixedAccessObjectModule mixed_access;
    ParameterObject mixed_object;
    assert(mixed_access.getParameter("mixed", mixed_object) == -EACCES);
    assert(mixed_access.getParameter("mixed/value", integer) == 0
           && integer == 1);
    assert(mixed_access.getParameter("mixed/write-only", integer)
           == -EACCES);
    const uint64_t mixed_revision = mixed_access.parameterRevision();
    assert(mixed_access.setParameter(
               "mixed", ParameterObject({{"read-only", 8}}))
           == -EACCES);
    assert(mixed_access.setParameter(
               "mixed",
               ParameterObject({{"nested", {{"locked", 8}}}}))
           == -EACCES);
    assert(mixed_access.setParameter(
               "mixed", ParameterObject({{"state-limited", 8}}))
           == -EBUSY);
    assert(mixed_access.parameterRevision() == mixed_revision);
    mixed_access.setState(0x2);
    assert(mixed_access.setParameter(
               "mixed", ParameterObject({{"state-limited", 8}}))
           == 0);
    assert(mixed_access.setParameter(
               "mixed", ParameterObject({{"write-only", 9}}))
           == 0);
    assert(mixed_access.getParameter("mixed/state-limited", integer) == 0
           && integer == 8);

    IncompleteObjectGetterModule incomplete_getter;
    assert(incomplete_getter.installIncompleteGetter() == -EINVAL);
    assert(!incomplete_getter.hasParameter("object"));

#if OPENGL_SUPPORT
    const ImagePara image_input(1920, 1080, 1920, 1080,
                                V4L2_PIX_FMT_RGB32);
    const ImagePara image_output(1280, 720, 1280, 720,
                                 V4L2_PIX_FMT_NV12);
    ModuleImageProcessor image_processor(image_input, image_output);
    assert(image_processor.hasParameter("transform"));
    assert(image_processor.hasParameter("transform/crop/width"));
    assert(image_processor.hasParameter("output/format"));
    assert(image_processor.hasParameter("buffer-type"));

    assert(image_processor.queryParameter("transform/crop/x", info) == 0);
    assert((info.flags & PARAMETER_FLAG_RUNTIME) != 0);
    assert(image_processor.queryParameter("output/width", info) == 0);
    assert(info.apply_mode == ParameterApplyMode::RECONFIGURE);
    const ParameterStateMask output_states = (static_cast<ParameterStateMask>(1)
                                              << static_cast<int>(MediaStatus::CREATED))
                                             | (static_cast<ParameterStateMask>(1)
                                                << static_cast<int>(MediaStatus::STOPPED));
    assert(info.writable_states == output_states);
    const std::vector<ParameterInfo> output_members = image_processor.queryParameters("output");
    assert(output_members.size() == 6);
    for (const auto& member : output_members) {
        assert(member.apply_mode == ParameterApplyMode::RECONFIGURE);
        assert(member.writable_states == output_states);
    }

    assert(image_processor.setParameter(
               "transform",
               ParameterObject({
                   {"crop",
                    {{"x", 100}, {"y", 50}, {"width", 640}, {"height", 360}}},
                   {"rotation", static_cast<int>(ImageRotation::Rotate90)},
                   {"mirror", true},
                   {"flip", false},
               }))
           == 0);
    assert(image_processor.getParameter("transform/crop/x", integer) == 0
           && integer == 100);
    assert(image_processor.getParameterAsString("transform/rotation", text)
               == 0
           && text == "90");
    assert(image_processor.setParameter("transform/crop/x", 1800)
           == -ERANGE);
    assert(image_processor.getParameter("transform/crop/x", integer) == 0
           && integer == 100);

    assert(image_processor.setParameter("output/width", 640) == 0);
    assert(image_processor.getParameter("output/width", integer) == 0
           && integer == 640);
    assert(image_processor.setParameter("output/width", 0) == -EINVAL);
    assert(image_processor.setParameter(
               "buffer-type", VideoBuffer::DRM_BUFFER_CACHEABLE)
           == 0);
    assert(image_processor.setParameter(
               "buffer-type", VideoBuffer::MALLOC_BUFFER)
           == -EINVAL);
#endif

    return 0;
}

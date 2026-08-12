/*
 * Optional type-safe helpers for FFMedia parameters.
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once

#include <cerrno>
#include <limits>
#include <type_traits>
#include <utility>

#include "ff_media_parameter.hpp"
#include "base/pixel_fmt.hpp"

namespace FFMedia
{
inline int parameterObjectUint32(const ParameterObject& object,
                                 const std::string& name, uint32_t& value)
{
    int64_t integer = 0;
    const int ret = object.getMember(name, integer);
    if (ret < 0)
        return ret;
    if (integer < 0
        || static_cast<uint64_t>(integer)
               > std::numeric_limits<uint32_t>::max()) {
        return -ERANGE;
    }
    value = static_cast<uint32_t>(integer);
    return 0;
}

inline ParameterObject imageParaToParameterObject(const ImagePara& value)
{
    return ParameterObject({
        {"width", static_cast<int64_t>(value.width)},
        {"height", static_cast<int64_t>(value.height)},
        {"hstride", static_cast<int64_t>(value.hstride)},
        {"vstride", static_cast<int64_t>(value.vstride)},
        {"format", static_cast<int64_t>(value.v4l2Fmt)},
        {"compression", static_cast<int>(value.compression)},
    });
}

inline int parameterObjectToImagePara(const ParameterObject& object,
                                      ImagePara& value)
{
    int64_t compression = 0;
    int ret = parameterObjectUint32(object, "width", value.width);
    if (ret == 0)
        ret = parameterObjectUint32(object, "height", value.height);
    if (ret == 0)
        ret = parameterObjectUint32(object, "hstride", value.hstride);
    if (ret == 0)
        ret = parameterObjectUint32(object, "vstride", value.vstride);
    if (ret == 0)
        ret = parameterObjectUint32(object, "format", value.v4l2Fmt);
    if (ret == 0)
        ret = object.getMember("compression", compression);
    if (ret < 0)
        return ret;
    if (compression < static_cast<int>(ImageCompression::Linear)
        || compression > static_cast<int>(ImageCompression::Afbc16x16)) {
        return -EINVAL;
    }
    value.compression = static_cast<ImageCompression>(compression);
    return 0;
}

inline ParameterInfo imageParaParameter(
    const std::string& name, const ImagePara& value,
    const std::string& description = "Image parameters",
    uint32_t flags = PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE,
    bool atomic = true)
{
    const int64_t maximum = static_cast<int64_t>(std::numeric_limits<uint32_t>::max());
    ParameterInfo compression = integerParameter(
        "compression", static_cast<int>(value.compression),
        static_cast<int>(ImageCompression::Linear),
        static_cast<int>(ImageCompression::Afbc16x16));
    compression.enum_values = {
        ParameterEnumValue("linear",
                           static_cast<int>(ImageCompression::Linear)),
        ParameterEnumValue("afbc-16x16",
                           static_cast<int>(ImageCompression::Afbc16x16)),
    };
    return objectParameter(
        name,
        {
            integerParameter("width", value.width, 0, maximum),
            integerParameter("height", value.height, 0, maximum),
            integerParameter("hstride", value.hstride, 0, maximum),
            integerParameter("vstride", value.vstride, 0, maximum),
            integerParameter("format", value.v4l2Fmt, 0, maximum),
            compression,
        },
        description, flags, atomic);
}

inline ParameterObject imageCropToParameterObject(const ImageCrop& value)
{
    return ParameterObject({
        {"x", static_cast<int64_t>(value.x)},
        {"y", static_cast<int64_t>(value.y)},
        {"width", static_cast<int64_t>(value.w)},
        {"height", static_cast<int64_t>(value.h)},
    });
}

inline int parameterObjectToImageCrop(const ParameterObject& object,
                                      ImageCrop& value)
{
    int ret = parameterObjectUint32(object, "x", value.x);
    if (ret == 0)
        ret = parameterObjectUint32(object, "y", value.y);
    if (ret == 0)
        ret = parameterObjectUint32(object, "width", value.w);
    if (ret == 0)
        ret = parameterObjectUint32(object, "height", value.h);
    return ret;
}

inline ParameterInfo imageCropParameter(
    const std::string& name, const ImageCrop& value = ImageCrop{},
    const std::string& description = "Image crop",
    uint32_t flags = PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE,
    bool atomic = true)
{
    const int64_t maximum = static_cast<int64_t>(std::numeric_limits<uint32_t>::max());
    return objectParameter(
        name,
        {
            integerParameter("x", value.x, 0, maximum),
            integerParameter("y", value.y, 0, maximum),
            integerParameter("width", value.w, 0, maximum),
            integerParameter("height", value.h, 0, maximum),
        },
        description, flags, atomic);
}

inline ParameterObject sampleInfoToParameterObject(const SampleInfo& value)
{
    return ParameterObject({
        {"format", static_cast<int>(value.fmt)},
        {"channels", value.channels},
        {"sample-rate", value.sample_rate},
        {"samples", value.nb_samples},
    });
}

inline int parameterObjectToSampleInfo(const ParameterObject& object,
                                       SampleInfo& value)
{
    int64_t format = 0;
    int64_t channels = 0;
    int64_t sample_rate = 0;
    int64_t samples = 0;
    int ret = object.getMember("format", format);
    if (ret == 0)
        ret = object.getMember("channels", channels);
    if (ret == 0)
        ret = object.getMember("sample-rate", sample_rate);
    if (ret == 0)
        ret = object.getMember("samples", samples);
    if (ret < 0)
        return ret;
    if (format < SAMPLE_FMT_NONE || format >= SAMPLE_FMT_NB
        || channels < 0 || channels > std::numeric_limits<int>::max()
        || sample_rate < 0 || sample_rate > std::numeric_limits<int>::max()
        || samples < 0 || samples > std::numeric_limits<int>::max()) {
        return -ERANGE;
    }
    value.fmt = static_cast<SampleFormat>(format);
    value.channels = static_cast<int>(channels);
    value.sample_rate = static_cast<int>(sample_rate);
    value.nb_samples = static_cast<int>(samples);
    return 0;
}

inline ParameterInfo sampleInfoParameter(
    const std::string& name, const SampleInfo& value = SampleInfo(),
    const std::string& description = "Audio sample parameters",
    uint32_t flags = PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE,
    bool atomic = true)
{
    return objectParameter(
        name,
        {
            integerParameter("format", value.fmt, SAMPLE_FMT_NONE,
                             SAMPLE_FMT_NB - 1),
            integerParameter("channels", value.channels, 0,
                             std::numeric_limits<int>::max()),
            integerParameter("sample-rate", value.sample_rate, 0,
                             std::numeric_limits<int>::max(), "", "Hz"),
            integerParameter("samples", value.nb_samples, 0,
                             std::numeric_limits<int>::max()),
        },
        description, flags, atomic);
}

namespace detail
{

    template <typename T, typename Enable = void>
    struct ParameterBindingTraits;

    template <typename T>
    struct ParameterBindingTraits<
        T, typename std::enable_if<std::is_integral<T>::value
                                   && !std::is_same<T, bool>::value>::type> {
        static ParameterValue get(T value)
        {
            if (!std::is_signed<T>::value
                && static_cast<uint64_t>(value)
                       > static_cast<uint64_t>(
                           std::numeric_limits<int64_t>::max())) {
                return ParameterValue();
            }
            return ParameterValue(static_cast<int64_t>(value));
        }

        static bool set(const ParameterValue& input, T& value)
        {
            int64_t integer = 0;
            if (!input.getInteger(integer))
                return false;

            if (std::is_signed<T>::value) {
                if (integer < static_cast<int64_t>(
                        std::numeric_limits<T>::min())
                    || integer > static_cast<int64_t>(
                           std::numeric_limits<T>::max())) {
                    return false;
                }
            } else if (integer < 0
                       || static_cast<uint64_t>(integer)
                              > static_cast<uint64_t>(
                                  std::numeric_limits<T>::max())) {
                return false;
            }

            value = static_cast<T>(integer);
            return true;
        }
    };

    template <typename T>
    struct ParameterBindingTraits<
        T, typename std::enable_if<std::is_enum<T>::value>::type> {
        using Underlying = typename std::underlying_type<T>::type;

        static ParameterValue get(T value)
        {
            return ParameterBindingTraits<Underlying>::get(
                static_cast<Underlying>(value));
        }

        static bool set(const ParameterValue& input, T& value)
        {
            Underlying integer{};
            if (!ParameterBindingTraits<Underlying>::set(input, integer))
                return false;
            value = static_cast<T>(integer);
            return true;
        }
    };

    template <typename T>
    struct ParameterBindingTraits<
        T, typename std::enable_if<std::is_floating_point<T>::value>::type> {
        static ParameterValue get(T value)
        {
            return ParameterValue(static_cast<double>(value));
        }

        static bool set(const ParameterValue& input, T& value)
        {
            double number = 0.0;
            if (!input.getDouble(number)
                || number < static_cast<double>(
                       std::numeric_limits<T>::lowest())
                || number > static_cast<double>(
                       std::numeric_limits<T>::max())) {
                return false;
            }
            value = static_cast<T>(number);
            return true;
        }
    };

    template <>
    struct ParameterBindingTraits<bool, void> {
        static ParameterValue get(bool value) { return ParameterValue(value); }
        static bool set(const ParameterValue& input, bool& value)
        {
            return input.getBoolean(value);
        }
    };

    template <>
    struct ParameterBindingTraits<std::string, void> {
        static ParameterValue get(const std::string& value)
        {
            return ParameterValue(value);
        }
        static bool set(const ParameterValue& input, std::string& value)
        {
            return input.getString(value);
        }
    };

    template <>
    struct ParameterBindingTraits<ParameterValue, void> {
        static ParameterValue get(const ParameterValue& value) { return value; }
        static bool set(const ParameterValue& input, ParameterValue& value)
        {
            value = input;
            return true;
        }
    };

    template <typename Result>
    struct ParameterSetterInvoker {
        template <typename Setter, typename Value>
        static int invoke(Setter& setter, Value& value)
        {
            static_assert(
                std::is_same<typename std::decay<Result>::type, int>::value,
                "Parameter setter must return int or void");
            return static_cast<int>(setter(value));
        }
    };

    template <>
    struct ParameterSetterInvoker<void> {
        template <typename Setter, typename Value>
        static int invoke(Setter& setter, Value& value)
        {
            setter(value);
            return 0;
        }
    };

}  // namespace detail

/** Bind a schema member directly to mutable instance storage. */
template <typename T>
ParameterBinding bindParameter(const std::string& path, T& value)
{
    static_assert(!std::is_const<T>::value && !std::is_volatile<T>::value,
                  "Parameter storage must be mutable");
    using ValueType = typename std::remove_cv<T>::type;
    using Traits = detail::ParameterBindingTraits<ValueType>;
    ValueType* storage = &value;

    ParameterBinding binding;
    binding.path = path;
    binding.getter = [storage](ParameterValue& output) {
        output = Traits::get(*storage);
        return output.valid() ? 0 : -EINVAL;
    };
    binding.setter = [storage](const ParameterValue& input) {
        return Traits::set(input, *storage) ? 0 : -EINVAL;
    };
    return binding;
}

/** Bind a schema member to a member of a derived, Impl, or external object. */
template <typename Object, typename Member>
ParameterBinding bindParameter(const std::string& path, Object& object,
                               Member Object::*member)
{
    return bindParameter(path, object.*member);
}

/** Bind through typed accessors for validation, state checks, or recreation. */
template <typename Getter, typename Setter>
typename std::enable_if<
    !std::is_member_pointer<typename std::decay<Setter>::type>::value,
    ParameterBinding>::type
bindParameter(const std::string& path, Getter getter, Setter setter)
{
    using ValueType = typename std::decay<
        decltype(std::declval<Getter&>()())>::type;
    using Traits = detail::ParameterBindingTraits<ValueType>;
    using SetterResult = decltype(std::declval<Setter&>()(std::declval<ValueType&>()));

    ParameterBinding binding;
    binding.path = path;
    binding.getter = [getter](ParameterValue& output) mutable {
        output = Traits::get(getter());
        return output.valid() ? 0 : -EINVAL;
    };
    binding.setter = [setter](const ParameterValue& input) mutable {
        ValueType value{};
        if (!Traits::set(input, value))
            return -EINVAL;
        return detail::ParameterSetterInvoker<SetterResult>::invoke(
            setter, value);
    };
    return binding;
}

inline ParameterObjectBinding bindParameterObject(
    const std::string& path, ParameterObjectGetter getter,
    ParameterObjectSetter setter)
{
    ParameterObjectBinding binding;
    binding.path = path;
    binding.getter = std::move(getter);
    binding.setter = std::move(setter);
    return binding;
}

}  // namespace FFMedia

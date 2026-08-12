/*
 * Generic typed parameter configuration support for FFMedia modules.
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/ff_type.hpp"

namespace FFMedia
{

class ParameterObject;
struct ParameterMemberValue;

enum class ParameterType : int32_t {
    INVALID = 0,
    BOOLEAN,
    INTEGER,
    DOUBLE,
    STRING,
    OBJECT,
};

enum ParameterFlag : uint32_t {
    PARAMETER_FLAG_READABLE = 1u << 0,
    PARAMETER_FLAG_WRITABLE = 1u << 1,
    PARAMETER_FLAG_RUNTIME = 1u << 2,
    PARAMETER_FLAG_DEPRECATED = 1u << 3,
};

enum class ParameterApplyMode : int32_t {
    IMMEDIATE = 0,
    RECONFIGURE,
    NEXT_START,
    CONSTRUCT_ONLY,
};

using ParameterStateMask = uint64_t;
static const ParameterStateMask PARAMETER_STATE_ANY = ~static_cast<ParameterStateMask>(0);

class FFMEDIA_API ParameterValue
{
public:
    ParameterValue();
    explicit ParameterValue(bool value);
    explicit ParameterValue(int value);
    explicit ParameterValue(int64_t value);
    explicit ParameterValue(double value);
    explicit ParameterValue(const char* value);
    explicit ParameterValue(const std::string& value);
    explicit ParameterValue(const ParameterObject& value);

    ParameterType type() const { return type_; }
    bool valid() const { return type_ != ParameterType::INVALID; }

    bool getBoolean(bool& value) const;
    bool getInteger(int64_t& value) const;
    bool getDouble(double& value) const;
    bool getString(std::string& value) const;
    bool getObject(ParameterObject& value) const;

    std::string toString() const;
    static int fromString(ParameterType type, const std::string& text,
                          ParameterValue& value);

private:
    ParameterType type_;
    bool boolean_value_;
    int64_t integer_value_;
    double double_value_;
    std::string string_value_;
    std::shared_ptr<ParameterObject> object_value_;
};

struct ParameterEnumValue {
    ParameterEnumValue()
        : value(0) {}
    ParameterEnumValue(const std::string& name_, int64_t value_,
                       const std::string& description_ = "")
        : name(name_), value(value_), description(description_)
    {
    }

    std::string name;
    int64_t value;
    std::string description;
};

/**
 * One node in an immutable parameter tree, similar to GParamSpec/AVOption.
 * OBJECT nodes replace the old target/group abstraction and may be atomic.
 */
struct ParameterInfo {
    ParameterInfo()
        : type(ParameterType::INVALID),
          flags(PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE),
          has_minimum(false), has_maximum(false), atomic(false),
          writable_states(PARAMETER_STATE_ANY),
          apply_mode(ParameterApplyMode::IMMEDIATE)
    {
    }

    std::string name;
    std::string description;
    std::string unit;
    ParameterType type;
    uint32_t flags;
    ParameterValue default_value;
    ParameterValue minimum;
    ParameterValue maximum;
    bool has_minimum;
    bool has_maximum;
    std::vector<ParameterEnumValue> enum_values;
    std::vector<ParameterInfo> object_members;
    /** Atomic OBJECT nodes are committed through one object setter or storage. */
    bool atomic;
    ParameterStateMask writable_states;
    ParameterApplyMode apply_mode;
};

struct FFMEDIA_API ParameterMemberValue {
    ParameterMemberValue() {}
    ParameterMemberValue(const std::string& name_,
                         const ParameterValue& value_)
        : name(name_), value(value_) {}
    ParameterMemberValue(const std::string& name_, bool value_)
        : name(name_), value(ParameterValue(value_)) {}
    ParameterMemberValue(const std::string& name_, int value_)
        : name(name_), value(ParameterValue(value_)) {}
    ParameterMemberValue(const std::string& name_, int64_t value_)
        : name(name_), value(ParameterValue(value_)) {}
    ParameterMemberValue(const std::string& name_, double value_)
        : name(name_), value(ParameterValue(value_)) {}
    ParameterMemberValue(const std::string& name_, const char* value_)
        : name(name_), value(ParameterValue(value_)) {}
    ParameterMemberValue(const std::string& name_,
                         const std::string& value_)
        : name(name_), value(ParameterValue(value_)) {}
    ParameterMemberValue(
        const std::string& name_,
        std::initializer_list<ParameterMemberValue> members_);
    ParameterMemberValue(const std::string& name_,
                         const ParameterObject& value_);

    std::string name;
    ParameterValue value;
};

/** A complete object value or a partial object patch. */
class FFMEDIA_API ParameterObject
{
public:
    ParameterObject();
    ParameterObject(std::initializer_list<ParameterMemberValue> members);

    bool empty() const { return members_.empty(); }
    bool hasMember(const std::string& member) const;
    std::vector<ParameterMemberValue> members() const;

    void setMember(const std::string& member, const ParameterValue& value);
    void setMember(const std::string& member, bool value);
    void setMember(const std::string& member, int value);
    void setMember(const std::string& member, int64_t value);
    void setMember(const std::string& member, double value);
    void setMember(const std::string& member, const char* value);
    void setMember(const std::string& member, const std::string& value);
    void setMember(const std::string& member, const ParameterObject& value);

    int getMember(const std::string& member, ParameterValue& value) const;
    int getMember(const std::string& member, bool& value) const;
    int getMember(const std::string& member, int64_t& value) const;
    int getMember(const std::string& member, double& value) const;
    int getMember(const std::string& member, std::string& value) const;
    int getMember(const std::string& member, ParameterObject& value) const;

private:
    std::map<std::string, ParameterValue> members_;
};

FFMEDIA_API ParameterInfo integerParameter(
    const std::string& name, int64_t default_value,
    int64_t minimum, int64_t maximum,
    const std::string& description = "", const std::string& unit = "",
    uint32_t flags = PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE);

FFMEDIA_API ParameterInfo booleanParameter(
    const std::string& name, bool default_value,
    const std::string& description = "",
    uint32_t flags = PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE);

FFMEDIA_API ParameterInfo doubleParameter(
    const std::string& name, double default_value,
    double minimum, double maximum,
    const std::string& description = "", const std::string& unit = "",
    uint32_t flags = PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE);

FFMEDIA_API ParameterInfo stringParameter(
    const std::string& name, const std::string& default_value,
    const std::string& description = "",
    uint32_t flags = PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE);

FFMEDIA_API ParameterInfo objectParameter(
    const std::string& name,
    std::initializer_list<ParameterInfo> members,
    const std::string& description = "",
    uint32_t flags = PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE,
    bool atomic = false);

/** Immutable class-level parameter tree shared by module instances. */
class FFMEDIA_API ParameterSchema
{
public:
    explicit ParameterSchema(std::initializer_list<ParameterInfo> parameters);

    const std::vector<ParameterInfo>& parameters() const
    {
        return parameters_;
    }

private:
    std::vector<ParameterInfo> parameters_;
};

FFMEDIA_API std::shared_ptr<const ParameterSchema> makeParameterSchema(
    std::initializer_list<ParameterInfo> parameters);

using ParameterValueGetter = std::function<int(ParameterValue& value)>;
using ParameterValueSetter = std::function<int(const ParameterValue& value)>;
using ParameterObjectGetter = std::function<int(ParameterObject& value)>;
using ParameterObjectSetter = std::function<int(const ParameterObject& value)>;
using ParameterStateGetter = std::function<ParameterStateMask()>;

/** A leaf path bound to instance storage or typed accessors. */
struct ParameterBinding {
    std::string path;
    ParameterValueGetter getter;
    ParameterValueSetter setter;
};

/** An OBJECT path bound to a complete transactional getter/setter. */
struct ParameterObjectBinding {
    std::string path;
    ParameterObjectGetter getter;
    ParameterObjectSetter setter;
};

/**
 * Thread-safe typed parameter tree.
 *
 * Paths use '/' as the hierarchy separator: "bitrate", "sample/channels".
 * A set on an atomic OBJECT or any descendant is merged and committed once.
 * Binding callbacks run without the internal tree lock. During one callback,
 * cross-thread parameter operations return -EAGAIN, reentrant reads are
 * allowed, getter dependency cycles return -ELOOP, and reentrant writes
 * return -EDEADLK.
 */
class FFMEDIA_API MediaParameter
{
public:
    MediaParameter();
    virtual ~MediaParameter();

    MediaParameter(const MediaParameter&) = delete;
    MediaParameter& operator=(const MediaParameter&) = delete;

    bool hasParameter(const std::string& path) const;
    int queryParameter(const std::string& path, ParameterInfo& info) const;
    std::vector<ParameterInfo> queryParameters(
        const std::string& parent_path = "") const;
    uint64_t parameterRevision() const;

    int getParameter(const std::string& path, ParameterValue& value) const;
    int getParameter(const std::string& path, bool& value) const;
    int getParameter(const std::string& path, int64_t& value) const;
    int getParameter(const std::string& path, double& value) const;
    int getParameter(const std::string& path, std::string& value) const;
    int getParameter(const std::string& path, ParameterObject& value) const;
    int getParameterAsString(const std::string& path,
                             std::string& value) const;

    int setParameter(const std::string& path, const ParameterValue& value);
    int setParameter(const std::string& path, bool value);
    int setParameter(const std::string& path, int value);
    int setParameter(const std::string& path, int64_t value);
    int setParameter(const std::string& path, double value);
    int setParameter(const std::string& path, const char* value);
    int setParameter(const std::string& path, const std::string& value);
    int setParameter(const std::string& path, const ParameterObject& value);
    int setParameterFromString(const std::string& path,
                               const std::string& value);

protected:
    int installParameterSchema(
        const std::shared_ptr<const ParameterSchema>& schema,
        std::initializer_list<ParameterBinding> bindings = {},
        std::initializer_list<ParameterObjectBinding> object_bindings = {});
    void setParameterStateGetter(ParameterStateGetter getter);
    void clearParameters();

private:
    int getParameterValue(const std::string& path,
                          ParameterValue& value) const;
    int setParameterValue(const std::string& path,
                          const ParameterValue& value);

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace FFMedia

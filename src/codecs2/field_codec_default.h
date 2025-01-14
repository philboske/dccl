// Copyright 2014-2023:
//   GobySoft, LLC (2013-)
//   Massachusetts Institute of Technology (2007-2014)
//   Community contributors (see AUTHORS file)
// File authors:
//   Toby Schneider <toby@gobysoft.org>
//   Davide Fenucci <davfen@noc.ac.uk>
//   Kyle Guilbert <kguilbert@aphysci.com>
//   Nathan Knotts <nknotts@gmail.com>
//   Chris Murphy <cmurphy@aphysci.com>
//
//
// This file is part of the Dynamic Compact Control Language Library
// ("DCCL").
//
// DCCL is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2.1 of the License, or
// (at your option) any later version.
//
// DCCL is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with DCCL.  If not, see <http://www.gnu.org/licenses/>.
// implements FieldCodecBase for all the basic DCCL types

#ifndef DCCLFIELDCODECDEFAULT20110322H
#define DCCLFIELDCODECDEFAULT20110322H

#include <sys/time.h>

#include <google/protobuf/descriptor.h>

#include "dccl/option_extensions.pb.h"

#include "../binary.h"
#include "../field_codec.h"
#include "../field_codec_fixed.h"
#include "field_codec_default_message.h"

namespace dccl
{
/// Goby/DCCL version 2 default field codecs
namespace v2
{
/// \brief Provides a basic bounded arbitrary length numeric (double, float, uint32, uint64, int32, int64) encoder.
///
/// Takes ceil(log2((max-min)*10^precision)+1) bits for required fields, ceil(log2((max-min)*10^precision)+2) for optional fields.
template <typename WireType, typename FieldType = WireType>
class DefaultNumericFieldCodec : public TypedFixedFieldCodec<WireType, FieldType>
{
  public:
    virtual double max()
    {
        DynamicConditions& dc = this->dynamic_conditions(this->this_field());

        double static_max = this->dccl_field_options().max();
        if (dc.has_max())
        {
            dc.regenerate(this->this_message(), this->root_message());
            // don't let dynamic conditions breach static bounds
            return std::max(this->dccl_field_options().min(), std::min(dc.max(), static_max));
        }
        else
        {
            return static_max;
        }
    }

    virtual double min()
    {
        DynamicConditions& dc = this->dynamic_conditions(this->this_field());
        double static_min = this->dccl_field_options().min();
        if (dc.has_min())
        {
            dc.regenerate(this->this_message(), this->root_message());

            // don't let dynamic conditions breach static bounds
            return std::min(this->dccl_field_options().max(), std::max(dc.min(), static_min));
        }
        else
        {
            return static_min;
        }
    }

    virtual double precision() { return FieldCodecBase::dccl_field_options().precision(); }

    virtual double resolution()
    {
        if (FieldCodecBase::dccl_field_options().has_precision())
            return std::pow(10.0, -precision());
        // If none is set returns the default resolution (=1)
        return FieldCodecBase::dccl_field_options().resolution();
    }

    void validate() override
    {
        FieldCodecBase::require(FieldCodecBase::dccl_field_options().has_min(),
                                "missing (dccl.field).min");
        FieldCodecBase::require(FieldCodecBase::dccl_field_options().has_max(),
                                "missing (dccl.field).max");

        FieldCodecBase::require(FieldCodecBase::dccl_field_options().resolution() > 0,
                                "(dccl.field).resolution must be greater than 0");
        FieldCodecBase::require(
            !(FieldCodecBase::dccl_field_options().has_precision() &&
              FieldCodecBase::dccl_field_options().has_resolution()),
            "at most one of either (dccl.field).precision or (dccl.field).resolution can be set");

        validate_numeric_bounds();
    }

    void validate_numeric_bounds()
    {
        // ensure given max and min fit within WireType ranges
        FieldCodecBase::require(static_cast<WireType>(min()) >=
                                    std::numeric_limits<WireType>::lowest(),
                                "(dccl.field).min must be >= minimum of this field type.");
        FieldCodecBase::require(static_cast<WireType>(max()) <=
                                    std::numeric_limits<WireType>::max(),
                                "(dccl.field).max must be <= maximum of this field type.");

        // allowable epsilon for min / max to diverge from nearest quantile
        const double min_max_eps = 1e-10;
        bool min_multiple_of_res = std::abs(quantize(min(), resolution()) - min()) < min_max_eps;
        bool max_multiple_of_res = std::abs(quantize(max(), resolution()) - max()) < min_max_eps;
        if (FieldCodecBase::dccl_field_options().has_resolution())
        {
            // ensure that max and min are multiples of the resolution chosen
            FieldCodecBase::require(
                min_multiple_of_res,
                "(dccl.field).min must be an exact multiple of (dccl.field).resolution");
            FieldCodecBase::require(
                max_multiple_of_res,
                "(dccl.field).max must be an exact multiple of (dccl.field).resolution");
        }
        else
        {
            auto res = resolution();
            // this was previously allowed so we will only give a warning not throw an exception
            if (!min_multiple_of_res)
                dccl::dlog.is(dccl::logger::WARN, dccl::logger::GENERAL) &&
                    dccl::dlog << "Warning: (dccl.field).min should be an exact multiple of "
                                  "10^(-(dccl.field).precision), i.e. "
                               << res << ": " << this->this_field()->DebugString() << std::endl;
            if (!max_multiple_of_res)
                dccl::dlog.is(dccl::logger::WARN, dccl::logger::GENERAL) &&
                    dccl::dlog << "Warning: (dccl.field).max should be an exact multiple of "
                                  "10^(-(dccl.field).precision), i.e. "
                               << res << ": " << this->this_field()->DebugString() << std::endl;
        }

        // ensure value fits into double
        FieldCodecBase::require(std::log2(max() - min()) - std::log2(resolution()) <=
                                    std::numeric_limits<double>::digits,
                                "[(dccl.field).max-(dccl.field).min]/(dccl.field).resolution must "
                                "fit in a double-precision floating point value. Please increase "
                                "min, decrease max, or decrease precision.");
    }

    Bitset encode() override { return Bitset(size()); }

    Bitset encode(const WireType& value) override
    {
        dccl::dlog.is(dccl::logger::DEBUG2, dccl::logger::ENCODE) &&
            dlog << "Encode " << value << " with bounds: [" << min() << "," << max() << "]"
                 << std::endl;

        // round first, before checking bounds
        double res = resolution();
        WireType wire_value = dccl::quantize(value, res);

        // check bounds
        if (wire_value < min() || wire_value > max())
        {
            // strict mode
            if (this->strict())
                throw(dccl::OutOfRangeException(
                    std::string("Value exceeds min/max bounds for field: ") +
                        FieldCodecBase::this_field()->DebugString(),
                    this->this_field(), this->this_descriptor()));
            // non-strict (default): if out-of-bounds, send as zeros
            else
                return Bitset(size());
        }

        // calculate the encoded value: remove the minimum, scale for the resolution, cast to int.
        wire_value -= dccl::quantize(static_cast<WireType>(min()), res);
        if (res >= 1)
            wire_value /= res;
        else
            wire_value *= (1.0 / res);
        auto uint_value = static_cast<dccl::uint64>(dccl::round(wire_value, 0));

        // "presence" value (0)
        if (!FieldCodecBase::use_required())
            uint_value += 1;

        Bitset encoded;
        encoded.from(uint_value, size());
        return encoded;
    }

    WireType decode(Bitset* bits) override
    {
        dccl::dlog.is(dccl::logger::DEBUG2, dccl::logger::DECODE) &&
            dlog << "Decode with bounds: [" << min() << "," << max() << "]" << std::endl;

        // The line below SHOULD BE:
        // dccl::uint64 t = bits->to<dccl::uint64>();
        // But GCC3.3 requires an explicit template modifier on the method.
        // See, e.g., http://gcc.gnu.org/bugzilla/show_bug.cgi?id=10959
        dccl::uint64 uint_value = (bits->template to<dccl::uint64>)();

        if (!FieldCodecBase::use_required())
        {
            if (!uint_value)
                throw NullValueException();
            --uint_value;
        }

        auto wire_value = (WireType)uint_value;
        double res = resolution();
        if (res >= 1)
            wire_value *= res;
        else
            wire_value /= (1.0 / res);

        // round values again to properly handle cases where double precision
        // leads to slightly off values (e.g. 2.099999999 instead of 2.1)
        wire_value =
            dccl::quantize(wire_value + dccl::quantize(static_cast<WireType>(min()), res), res);
        return wire_value;
    }

    // bring size(const WireType&) into scope so callers can access it
    using TypedFixedFieldCodec<WireType, FieldType>::size;

    unsigned size() override
    {
        // if not required field, leave one value for unspecified (always encoded as 0)
        unsigned NULL_VALUE = FieldCodecBase::use_required() ? 0 : 1;

        return dccl::ceil_log2((max() - min()) / resolution() + 1 + NULL_VALUE);
    }
};

/// \brief Provides a bool encoder. Uses 1 bit if field is `required`, 2 bits if `optional`
///
/// [presence bit (0 bits if required, 1 bit if optional)][value (1 bit)]
class DefaultBoolCodec : public TypedFixedFieldCodec<bool>
{
  private:
    Bitset encode(const bool& wire_value) override;
    Bitset encode() override;
    bool decode(Bitset* bits) override;
    unsigned size() override;
    void validate() override;
};

/// \brief Provides an variable length ASCII string encoder. Can encode strings up to 255 bytes by using a length byte preceeding the string.
///
/// [length of following string (1 byte)][string (0-255 bytes)]
class DefaultStringCodec : public TypedFieldCodec<std::string>
{
  private:
    Bitset encode() override;
    Bitset encode(const std::string& wire_value) override;
    std::string decode(Bitset* bits) override;
    unsigned size() override;
    unsigned size(const std::string& wire_value) override;
    unsigned max_size() override;
    unsigned min_size() override;
    void validate() override;

  private:
    enum
    {
        MAX_STRING_LENGTH = 255
    };
};

/// \brief Provides an fixed length byte string encoder.
class DefaultBytesCodec : public TypedFieldCodec<std::string>
{
  private:
    Bitset encode() override;
    Bitset encode(const std::string& wire_value) override;
    std::string decode(Bitset* bits) override;
    unsigned size() override;
    unsigned size(const std::string& wire_value) override;
    unsigned max_size() override;
    unsigned min_size() override;
    void validate() override;
};

/// \brief Provides an enum encoder. This converts the enumeration to an integer (based on the enumeration <i>index</i> (<b>not</b> its <i>value</i>) and uses DefaultNumericFieldCodec to encode the integer.
class DefaultEnumCodec
    : public DefaultNumericFieldCodec<int32, const google::protobuf::EnumValueDescriptor*>
{
  public:
    int32 pre_encode(const google::protobuf::EnumValueDescriptor* const& field_value) override;
    const google::protobuf::EnumValueDescriptor* post_decode(const int32& wire_value) override;

  private:
    void validate() override {}

    double max() override
    {
        const google::protobuf::EnumDescriptor* e = this_field()->enum_type();
        return e->value_count() - 1;
    }
    double min() override { return 0; }
};

typedef double time_wire_type;
/// \brief Encodes time of day (default: second precision, but can be set with (dccl.field).precision extension)
///
/// \tparam TimeType A type representing time: See the various specializations of this class for allowed types.
template <typename TimeType, int conversion_factor>
class TimeCodecBase : public DefaultNumericFieldCodec<time_wire_type, TimeType>
{
  public:
    time_wire_type pre_encode(const TimeType& time_of_day) override
    {
        time_wire_type max_secs = max();
        return std::fmod(time_of_day / static_cast<time_wire_type>(conversion_factor), max_secs);
    }

    TimeType post_decode(const time_wire_type& encoded_time) override
    {
        auto max_secs = (int64)max();
        timeval t;
        gettimeofday(&t, nullptr);
        int64 now = t.tv_sec;
        int64 daystart = now - (now % max_secs);
        int64 today_time = now - daystart;

        // If time is more than 12 hours ahead of now, assume it's yesterday.
        if ((encoded_time - today_time) > (max_secs / 2))
        {
            daystart -= max_secs;
        }
        else if ((today_time - encoded_time) > (max_secs / 2))
        {
            daystart += max_secs;
        }

        return dccl::round((TimeType)(conversion_factor * (daystart + encoded_time)),
                           precision() - std::log10((double)conversion_factor));
    }

  private:
    void validate() override
    {
        DefaultNumericFieldCodec<time_wire_type, TimeType>::validate_numeric_bounds();
    }

    double max() override
    {
        return FieldCodecBase::dccl_field_options().num_days() * SECONDS_IN_DAY;
    }

    double min() override { return 0; }
    double precision() override
    {
        if (!FieldCodecBase::dccl_field_options().has_precision())
            return 0; // default to second precision
        else
        {
            return FieldCodecBase::dccl_field_options().precision() +
                   (double)std::log10((double)conversion_factor);
        }
    }

  private:
    enum
    {
        SECONDS_IN_DAY = 86400
    };
};

template <typename TimeType> class TimeCodec : public TimeCodecBase<TimeType, 0>
{
    static_assert(sizeof(TimeCodec) == 0, "Must use specialization of TimeCodec");
};

template <> class TimeCodec<uint64> : public TimeCodecBase<uint64, 1000000>
{
};
template <> class TimeCodec<int64> : public TimeCodecBase<int64, 1000000>
{
};
template <> class TimeCodec<double> : public TimeCodecBase<double, 1>
{
};

/// \brief Placeholder codec that takes no space on the wire (0 bits).
template <typename T> class StaticCodec : public TypedFixedFieldCodec<T>
{
    Bitset encode(const T&) override { return Bitset(size()); }

    Bitset encode() override { return Bitset(size()); }

    T decode(Bitset* /*bits*/) override
    {
        std::istringstream iss(FieldCodecBase::dccl_field_options().static_value());
        T value;
        iss >> value;
        return value;
    }

    unsigned size() override { return 0; }

    void validate() override
    {
        FieldCodecBase::require(FieldCodecBase::dccl_field_options().has_static_value(),
                                "missing (dccl.field).static_value");

        std::string t = FieldCodecBase::dccl_field_options().static_value();
        std::istringstream iss(t);
        T value;

        if (!(iss >> value))
        {
            FieldCodecBase::require(false, "invalid static_value for this type.");
        }
    }
};
} // namespace v2
} // namespace dccl

#endif

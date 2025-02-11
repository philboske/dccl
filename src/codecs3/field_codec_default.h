// Copyright 2014-2019:
//   GobySoft, LLC (2013-)
//   Massachusetts Institute of Technology (2007-2014)
//   Community contributors (see AUTHORS file)
// File authors:
//   Toby Schneider <toby@gobysoft.org>
//   Chris Murphy <cmurphy@aphysci.com>
//   Kyle Guilbert <kguilbert@aphysci.com>
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
// implements FieldCodecBase for all the basic DCCL types for version 3

#ifndef DCCLV3FIELDCODECDEFAULT20140403H
#define DCCLV3FIELDCODECDEFAULT20140403H

#include "../codecs2/field_codec_default.h"

namespace dccl
{
/// DCCL version 3 default field codecs
namespace v3
{
// all these are the same as version 2
template <typename WireType, typename FieldType = WireType>
class DefaultNumericFieldCodec : public v2::DefaultNumericFieldCodec<WireType, FieldType>
{
};

using DefaultBoolCodec = v2::DefaultBoolCodec;
using DefaultBytesCodec = v2::DefaultBytesCodec;
// Enum Codec is identical to v2, except when packed_enum is set to false.

/// \brief Provides an enum encoder. This converts the enumeration to an integer and uses
/// DefaultNumericFieldCodec to encode the integer.  Note that by default, the value is
/// based on the enumeration <i>index</i> (<b>not</b> its <i>value</i>.  If you wish to
/// allocate space for all values between a lower and upper bound (for future expansion
/// of the enumeration values, for instance) then set the (dccl.field).packed_enum
/// extension to false.  The enumerate value will then be used for encoding.
class DefaultEnumCodec
    : public DefaultNumericFieldCodec<int32, const google::protobuf::EnumValueDescriptor*>
{
  public:
    int32 pre_encode(const google::protobuf::EnumValueDescriptor* const& field_value) override;
    const google::protobuf::EnumValueDescriptor* post_decode(const int32& wire_value) override;
    void validate() override {}

  private:
    double max() override;
    double min() override;
};

template <typename TimeType> class TimeCodec : public v2::TimeCodecBase<TimeType, 0>
{
    static_assert(sizeof(TimeCodec) == 0, "Must use specialization of TimeCodec");
};

template <> class TimeCodec<uint64> : public v2::TimeCodecBase<uint64, 1000000>
{
};
template <> class TimeCodec<int64> : public v2::TimeCodecBase<int64, 1000000>
{
};
template <> class TimeCodec<double> : public v2::TimeCodecBase<double, 1>
{
};

template <typename T> class StaticCodec : public v2::StaticCodec<T>
{
};

/// \brief Provides an variable length ASCII string encoder.
///
/// [length of following string size: ceil(log2(max_length))][string]
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
};

} // namespace v3
} // namespace dccl

#endif

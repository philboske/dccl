// Copyright 2011-2023:
//   GobySoft, LLC (2013-)
//   Massachusetts Institute of Technology (2007-2014)
//   Community contributors (see AUTHORS file)
// File authors:
//   Toby Schneider <toby@gobysoft.org>
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

#include "field_codec_default_message.h"
#include "../codec.h"
#include "../oneof.h"

std::unordered_map<std::string, unsigned> dccl::v4::DefaultMessageCodec::MaxSize::oneofs_max_size;

//
// DefaultMessageCodec
//

void dccl::v4::DefaultMessageCodec::any_encode(Bitset* bits, const dccl::any& wire_value)
{
    if (is_empty(wire_value))
    {
        *bits = Bitset(min_size());
    }
    else
    {
        *bits = traverse_const_message<Encoder, Bitset>(wire_value);

        if (is_optional())
            bits->push_front(true); // presence bit
    }
}

unsigned dccl::v4::DefaultMessageCodec::any_size(const dccl::any& wire_value)
{
    if (is_empty(wire_value))
    {
        return min_size();
    }
    else
    {
        unsigned size = traverse_const_message<Size, unsigned>(wire_value);
        if (is_optional())
        {
            const unsigned presence_bit = 1;
            size += presence_bit;
        }

        return size;
    }
}

void dccl::v4::DefaultMessageCodec::any_decode(Bitset* bits, dccl::any* wire_value)
{
    try
    {
        auto* msg = dccl::any_cast<google::protobuf::Message*>(*wire_value);

        if (is_optional())
        {
            if (!bits->to_ulong())
            {
                *wire_value = dccl::any();
                return;
            }
            else
            {
                bits->pop_front(); // presence bit
            }
        }

        const google::protobuf::Descriptor* desc = msg->GetDescriptor();
        const google::protobuf::Reflection* refl = msg->GetReflection();

        // First, process the oneof definitions, storing the case value...
        std::vector<int> oneof_cases(desc->oneof_decl_count());
        for (auto i = 0, n = desc->oneof_decl_count(); part() != HEAD && i < n; ++i)
        {
            Bitset case_bits(bits);
            case_bits.get_more_bits(oneof_size(desc->oneof_decl(i)));

            // Store the index of the field set for the i-th oneof (if unset, it will be -1)
            oneof_cases[i] = static_cast<int>(case_bits.to_ulong()) - 1;
        }

        // ... then, process the fields
        for (int i = 0, n = desc->field_count(); i < n; ++i)
        {
            const google::protobuf::FieldDescriptor* field_desc = desc->field(i);

            if (!check_field(field_desc))
                continue;

            std::shared_ptr<FieldCodecBase> codec = find(field_desc);
            std::shared_ptr<internal::FromProtoCppTypeBase> helper =
                manager().type_helper().find(field_desc);

            if (field_desc->is_repeated())
            {
                std::vector<dccl::any> field_values;
                if (field_desc->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE)
                {
                    unsigned max_repeat =
                        field_desc->options().GetExtension(dccl::field).max_repeat();
                    for (unsigned j = 0, m = max_repeat; j < m; ++j)
                        field_values.emplace_back(refl->AddMessage(msg, field_desc));

                    codec->field_decode_repeated(bits, &field_values, field_desc);

                    // remove the unused messages
                    for (int j = field_values.size(), m = max_repeat; j < m; ++j)
                    {
                        refl->RemoveLast(msg, field_desc);
                    }
                }
                else
                {
                    // for primitive types
                    codec->field_decode_repeated(bits, &field_values, field_desc);
                    for (auto& field_value : field_values)
                        helper->add_value(field_desc, msg, field_value);
                }
            }
            else
            {
                if (is_part_of_oneof(field_desc))
                {
                    // If the field belongs to a oneof and its index is the one stored for the containing
                    // oneof, decode it; otherwise, skip the field.
                    if (field_desc->index_in_oneof() !=
                        oneof_cases[containing_oneof_index(field_desc)])
                        continue;
                }

                // singular field dynamic conditions - repeated fields handled in any_decode_repeated
                DynamicConditions& dc = dynamic_conditions(field_desc);
                if (dc.has_omit_if())
                {
                    // expensive, so don't do this unless we're going to use it
                    dc.regenerate(this_message(), root_message());
                    if (dc.omit())
                        continue;
                }

                dccl::any field_value;
                if (field_desc->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE)
                {
                    // allows us to propagate pointers instead of making many copies of entire messages
                    field_value = refl->MutableMessage(msg, field_desc);
                    codec->field_decode(bits, &field_value, field_desc);
                    if (is_empty(field_value))
                        refl->ClearField(msg, field_desc);
                }
                else
                {
                    // for primitive types
                    codec->field_decode(bits, &field_value, field_desc);
                    helper->set_value(field_desc, msg, field_value);
                }
            }
        }

        std::vector<const google::protobuf::FieldDescriptor*> set_fields;
        refl->ListFields(*msg, &set_fields);
        *wire_value = msg;
    }
    catch (dccl::bad_any_cast& e)
    {
        throw(Exception(
            "Bad type given to traverse mutable, expecting google::protobuf::Message*, got " +
            std::string(wire_value->type().name())));
    }
}

unsigned dccl::v4::DefaultMessageCodec::max_size()
{
    unsigned u = 0;
    traverse_descriptor<MaxSize>(&u);

    if (is_optional())
    {
        const unsigned presence_bit = 1;
        u += presence_bit;
    }

    return u;
}

unsigned dccl::v4::DefaultMessageCodec::min_size()
{
    if (is_optional())
    {
        const unsigned presence_bit = 1;
        return presence_bit;
    }
    else
    {
        unsigned u = 0;
        traverse_descriptor<MinSize>(&u);
        return u;
    }
}

void dccl::v4::DefaultMessageCodec::validate()
{
    bool b = false;
    traverse_descriptor<Validate>(&b);
}

std::string dccl::v4::DefaultMessageCodec::info()
{
    std::stringstream ss;
    traverse_descriptor<Info>(&ss);
    return ss.str();
}

bool dccl::v4::DefaultMessageCodec::check_field(const google::protobuf::FieldDescriptor* field)
{
    if (!field)
    {
        return true;
    }
    else
    {
        dccl::DCCLFieldOptions dccl_field_options = field->options().GetExtension(dccl::field);
        if (dccl_field_options.omit()) // omit
        {
            return false;
        }
        else if (message_data().current_part() == UNKNOWN) // part not yet explicitly specified
        {
            if ((part() == HEAD && !dccl_field_options.in_head()) ||
                (part() == BODY && dccl_field_options.in_head()))
                return false;
            else
                return true;
        }
        else if (message_data().current_part() != part()) // part specified and doesn't match
            return false;
        else
            return true;
    }
}

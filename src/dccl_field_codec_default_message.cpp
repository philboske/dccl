// Copyright 2009-2012 Toby Schneider (https://launchpad.net/~tes)
//                     Massachusetts Institute of Technology (2007-)
//                     Woods Hole Oceanographic Institution (2007-)
//                     Goby Developers Team (https://launchpad.net/~goby-dev)
// 
//
// This file is part of the Goby Underwater Autonomy Project Libraries
// ("The Goby Libraries").
//
// The Goby Libraries are free software: you can redistribute them and/or modify
// them under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// The Goby Libraries are distributed in the hope that they will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Goby.  If not, see <http://www.gnu.org/licenses/>.


#include "dccl/dccl.h"
#include "dccl_field_codec_default_message.h"
#include "goby/util/dynamic_protobuf_manager.h"

using dccl::dlog;

//
// DCCLDefaultMessageCodec
//

void dccl::DCCLDefaultMessageCodec::any_encode(Bitset* bits, const boost::any& wire_value)
{
  if(wire_value.empty())
      *bits = Bitset(min_size());
  else
      *bits = traverse_const_message<Encoder, Bitset>(wire_value);
}
  

 
unsigned dccl::DCCLDefaultMessageCodec::any_size(const boost::any& wire_value)
{
    if(wire_value.empty())
        return min_size();
    else
        return traverse_const_message<Size, unsigned>(wire_value);
}


void dccl::DCCLDefaultMessageCodec::any_decode(Bitset* bits, boost::any* wire_value)
{
    try
    {
        
        google::protobuf::Message* msg = boost::any_cast<google::protobuf::Message* >(*wire_value);
        
        const google::protobuf::Descriptor* desc = msg->GetDescriptor();
        const google::protobuf::Reflection* refl = msg->GetReflection();
        
        for(int i = 0, n = desc->field_count(); i < n; ++i)
        {
        
            const google::protobuf::FieldDescriptor* field_desc = desc->field(i);

            if(!check_field(field_desc))
                continue;

            boost::shared_ptr<DCCLFieldCodecBase> codec =
                DCCLFieldCodecManager::find(field_desc);
            boost::shared_ptr<FromProtoCppTypeBase> helper =
                DCCLTypeHelper::find(field_desc);

            if(field_desc->is_repeated())
            {   
                std::vector<boost::any> wire_values;
                if(field_desc->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE)
                {
                    for(unsigned j = 0, m = field_desc->options().GetExtension(dccl::field).max_repeat(); j < m; ++j)
                        wire_values.push_back(refl->AddMessage(msg, field_desc));
                    
                    codec->field_decode_repeated(bits, &wire_values, field_desc);

                    for(int j = 0, m = wire_values.size(); j < m; ++j)
                    {
                        if(wire_values[j].empty()) refl->RemoveLast(msg, field_desc);
                    }
                }
                else
                {
                    // for primitive types
                    codec->field_decode_repeated(bits, &wire_values, field_desc);
                    for(int j = 0, m = wire_values.size(); j < m; ++j)
                        helper->add_value(field_desc, msg, wire_values[j]);
                }
            }
            else
            {
                boost::any wire_value;
                if(field_desc->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE)
                {
                    // allows us to propagate pointers instead of making many copies of entire messages
                    wire_value = refl->MutableMessage(msg, field_desc);
                    codec->field_decode(bits, &wire_value, field_desc);
                    if(wire_value.empty()) refl->ClearField(msg, field_desc);    
                }
                else
                {
                    // for primitive types
                    codec->field_decode(bits, &wire_value, field_desc);
                    helper->set_value(field_desc, msg, wire_value);
                }
            } 
        }

        std::vector< const google::protobuf::FieldDescriptor* > set_fields;
        refl->ListFields(*msg, &set_fields);
        if(set_fields.empty() && this_field()) *wire_value = boost::any();
        else *wire_value = msg;
    }
    catch(boost::bad_any_cast& e)
    {
        throw(DCCLException("Bad type given to traverse mutable, expecting google::protobuf::Message*, got " + std::string(wire_value->type().name())));
    }

}


unsigned dccl::DCCLDefaultMessageCodec::max_size()
{
    unsigned u = 0;
    traverse_descriptor<MaxSize>(&u);
    return u;
}

unsigned dccl::DCCLDefaultMessageCodec::min_size()
{
    unsigned u = 0;
    traverse_descriptor<MinSize>(&u);
    return u;
}


void dccl::DCCLDefaultMessageCodec::validate()
{
    bool b = false;
    traverse_descriptor<Validate>(&b);
}

std::string dccl::DCCLDefaultMessageCodec::info()
{
    std::stringstream ss;
    traverse_descriptor<Info>(&ss);
    return ss.str();
}

bool dccl::DCCLDefaultMessageCodec::check_field(const google::protobuf::FieldDescriptor* field)
{
    if(!field)
    {
        return true;
    }
    else
    {
        dccl::DCCLFieldOptions dccl_field_options = field->options().GetExtension(dccl::field);
        if(dccl_field_options.omit()) // omit
        {
            return false;
        }
        else if(MessageHandler::current_part() == MessageHandler::UNKNOWN) // part not yet explicitly specified
        {
            if(field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE &&
               DCCLFieldCodecManager::find(field)->name() == DCCLCodec::DEFAULT_CODEC_NAME) // default message codec will expand
                return true;
            else if((part() == MessageHandler::HEAD && !dccl_field_options.in_head())
                    || (part() == MessageHandler::BODY && dccl_field_options.in_head()))
                return false;
            else
                return true;
        }
        else if(MessageHandler::current_part() != part()) // part specified and doesn't match
            return false;
        else
            return true;
    }    
}



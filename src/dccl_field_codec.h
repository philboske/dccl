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


#ifndef DCCLFIELDCODEC20110322H
#define DCCLFIELDCODEC20110322H

#include <map>
#include <string>

#include <boost/any.hpp>

#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>

#include "dccl_common.h"
#include "dccl_exception.h"
#include "dccl/protobuf/option_extensions.pb.h"
#include "dccl_type_helper.h"
#include "dccl_field_codec_helpers.h"

namespace dccl
{
    class Codec;

    /// \brief Provides a base class for defining DCCL field encoders / decoders. Most users will use the DCCLTypedFieldCodec or its children (e.g. DCCLTypedFixedFieldCodec) instead of directly inheriting from this class.
    class DCCLFieldCodecBase
    {
      public:
        /// \name Constructor, Destructor
        //@{
            
        DCCLFieldCodecBase();
        virtual ~DCCLFieldCodecBase() { }
        //@}
            
        /// \name Information Methods
        //@{
        /// \brief the name of the codec used to identifier it in the .proto custom option extension
        std::string name() const { return name_; }
        /// \brief the type exposed to the user in the original and decoded Protobuf messages
        ///
        /// \return the Google Protobuf message type. See http://code.google.com/apis/protocolbuffers/docs/reference/cpp/google.protobuf.descriptor.html#FieldDescriptor.Type.details
        google::protobuf::FieldDescriptor::Type field_type() const  { return field_type_; }
        /// \brief the C++ type used "on the wire". This is the type visible <i>after</i> pre_encode and <i>before</i> post_decode functions are called.
        ///
        /// The wire type allows codecs to make type changes (e.g. from string to integer) before reusing another codec that knows how to encode that wire type (e.g. DCCLDefaultNumericFieldCodec)
        /// \return the C++ type used to encode and decode. See http://code.google.com/apis/protocolbuffers/docs/reference/cpp/google.protobuf.descriptor.html#FieldDescriptor.CppType.details
        google::protobuf::FieldDescriptor::CppType wire_type() const  { return wire_type_; }


        /// \brief Returns the FieldDescriptor (field schema  meta-data) for this field
        ///
        /// \return FieldDescriptor for the current field or 0 if this codec is encoding the base message.
        const google::protobuf::FieldDescriptor* this_field() const 
        { return !MessageHandler::field_.empty() ? MessageHandler::field_.back() : 0; }
            
        /// \brief Returns the Descriptor (message schema meta-data) for the immediate parent Message
        ///
        /// for:
        /// \code
        /// message Foo
        /// {
        ///    int32 bar = 1;
        ///    FooBar baz = 2;
        /// }
        /// \endcode
        /// returns Descriptor for Foo if this_field() == 0
        /// returns Descriptor for Foo if this_field() == FieldDescriptor for bar
        /// returns Descriptor for FooBar if this_field() == FieldDescriptor for baz
        static const google::protobuf::Descriptor* this_descriptor()
        { return !MessageHandler::desc_.empty() ? MessageHandler::desc_.back() : 0; }

        // currently encoded or (partially) decoded root message
        static const google::protobuf::Message* root_message()
        { return root_message_; }
            
            
        /// \brief the part of the message currently being encoded (head or body).
        static MessageHandler::MessagePart part() { return part_; }
            
        //@}

        /// \name Base message functions
        ///
        /// These are called typically by Codec to start processing a new message. In this example "Foo" is a base message:
        /// \code
        /// message Foo
        /// {
        ///    int32 bar = 1;
        ///    FooBar baz = 2;
        /// }
        /// \endcode
        //@{

        /// \brief Encode this part (body or head) of the base message
        ///
        /// \param bits pointer to a Bitset where all bits will be pushed on to the most significant end.
        /// \param msg DCCL Message to encode
        /// \param part Part of the message to encode
        void base_encode(Bitset* bits,
                         const google::protobuf::Message& msg,
                         MessageHandler::MessagePart part);

        /// \brief Calculate the size (in bits) of a part of the base message when it is encoded
        ///
        /// \param bit_size Pointer to unsigned integer to store the result.
        /// \param msg the DCCL Message of which to calculate the size
        /// \param part part of the Message to calculate the size of
        void base_size(unsigned* bit_size, const google::protobuf::Message& msg, MessageHandler::MessagePart part);

        /// \brief Decode part of a message
        ///
        /// \param bits Pointer to a Bitset containing bits to decode. The least significant bits will be consumed first. Any bits not consumed will remain in `bits` after this method returns.
        /// \param msg DCCL Message to <i>merge</i> the decoded result into.
        /// \param part part of the Message to decode         
        void base_decode(Bitset* bits,
                         google::protobuf::Message* msg,
                         MessageHandler::MessagePart part);

        /// \brief Calculate the maximum size of a message given its Descriptor alone (no data)
        ///
        /// \param bit_size Pointer to unsigned integer to store calculated maximum size in bits.
        /// \param desc Descriptor to calculate the maximum size of. Use google::protobuf::Message::GetDescriptor() or MyProtobufType::descriptor() to get this object.
        /// \param part part of the Message 
        void base_max_size(unsigned* bit_size, const google::protobuf::Descriptor* desc, MessageHandler::MessagePart part);

        /// \brief Calculate the minimum size of a message given its Descriptor alone (no data)
        ///
        /// \param bit_size Pointer to unsigned integer to store calculated minimum size in bits.
        /// \param desc Descriptor to calculate the minimum size of. Use google::protobuf::Message::GetDescriptor() or MyProtobufType::descriptor() to get this object.
        /// \param part part of the Message
        void base_min_size(unsigned* bit_size, const google::protobuf::Descriptor* desc, MessageHandler::MessagePart part);

        /// \brief Validate this part of the message to make sure all required extensions are set.
        ///
        /// \param desc Descriptor to validate. Use google::protobuf::Message::GetDescriptor() or MyProtobufType::descriptor() to get this object.
        /// \param part part of the Message       
        void base_validate(const google::protobuf::Descriptor* desc, MessageHandler::MessagePart part);

        /// \brief Get human readable information (size of fields, etc.) about this part of the DCCL message
        /// 
        /// \param os Pointer to stream to store this information
        /// \param desc Descriptor to get information on. Use google::protobuf::Message::GetDescriptor() or MyProtobufType::descriptor() to get this object.
        /// \param part the part of the Message to act on.
        void base_info(std::ostream* os, const google::protobuf::Descriptor* desc, MessageHandler::MessagePart part);
        //@}
            
        /// \name Field functions (primitive types and embedded messages)
        // 
        /// These are called typically by DCCLDefaultMessageCodec to start processing a new field. In this example "bar" and "baz" are fields:
        /// \code
        /// message Foo
        /// {
        ///    int32 bar = 1;
        ///    FooBar baz = 2;
        /// }
        /// \endcode            
        //@{

        /// \brief Pre-encodes a non-repeated (i.e. optional or required) field by converting the FieldType representation (the Google Protobuf representation) into the WireType representation (the type used in the encoded DCCL message). This allows for type-converting codecs.
        ///
        /// \param wire_value Will be set to the converted field_value
        /// \param field_value Value to convert to the appropriate wire_value
        void field_pre_encode(boost::any* wire_value, const boost::any& field_value)
        { any_pre_encode(wire_value, field_value); }

        /// \brief Pre-encodes a repeated field.
        ///
        /// \param wire_values Should be set to the converted field_values
        /// \param field_values Values to convert to the appropriate wire_values
        void field_pre_encode_repeated(std::vector<boost::any>* wire_values,
                                       const std::vector<boost::any>& field_values)
        { any_pre_encode_repeated(wire_values, field_values); }
            
        // traverse const

        /// \brief Encode a non-repeated field.
        ///
        /// \param bits Pointer to bitset to store encoded bits. Bits are added to the most significant end of `bits`
        /// \param field_value Value to encode (FieldType)
        /// \param field Protobuf descriptor to the field to encode. Set to 0 for base message.
        void field_encode(Bitset* bits,
                          const boost::any& field_value,
                          const google::protobuf::FieldDescriptor* field);

        /// \brief Encode a repeated field.
        ///
        /// \param bits Pointer to bitset to store encoded bits. Bits are added to the most significant end of `bits`
        /// \param field_values Values to encode (FieldType)
        /// \param field Protobuf descriptor to the field. Set to 0 for base message.
        void field_encode_repeated(Bitset* bits,
                                   const std::vector<boost::any>& field_values,
                                   const google::protobuf::FieldDescriptor* field);

        /// \brief Calculate the size of a field
        ///
        /// \param bit_size Location to <i>add</i> calculated bit size to. Be sure to zero `bit_size` if you want only the size of this field.
        /// \param field_value Value calculate size of (FieldType)
        /// \param field Protobuf descriptor to the field. Set to 0 for base message.
        void field_size(unsigned* bit_size, const boost::any& field_value,
                        const google::protobuf::FieldDescriptor* field);
            
        /// \brief Calculate the size of a repeated field
        ///
        /// \param bit_size Location to <i>add</i> calculated bit size to. Be sure to zero `bit_size` if you want only the size of this field.
        /// \param field_values Values to calculate size of (FieldType)
        /// \param field Protobuf descriptor to the field. Set to 0 for base message.
        void field_size_repeated(unsigned* bit_size, const std::vector<boost::any>& wire_values,
                                 const google::protobuf::FieldDescriptor* field);

        // traverse mutable
        /// \brief Decode a non-repeated field
        ///
        /// \param bits Bits to decode. Used bits are consumed (erased) from the least significant end
        /// \param field_value Location to store decoded value (FieldType)
        /// \param field Protobuf descriptor to the field. Set to 0 for base message.
        void field_decode(Bitset* bits,
                          boost::any* field_value,
                          const google::protobuf::FieldDescriptor* field);            

        /// \brief Decode a repeated field
        ///
        /// \param bits Bits to decode. Used bits are consumed (erased) from the least significant end
        /// \param field_values Location to store decoded values (FieldType)
        /// \param field Protobuf descriptor to the field. Set to 0 for base message.
        void field_decode_repeated(Bitset* bits,
                                   std::vector<boost::any>* field_values,
                                   const google::protobuf::FieldDescriptor* field);

        /// \brief Post-decodes a non-repeated (i.e. optional or required) field by converting the WireType (the type used in the encoded DCCL message) representation into the FieldType representation (the Google Protobuf representation). This allows for type-converting codecs.
        ///
        /// \param wire_value Should be set to the desired value to translate
        /// \param field_value Will be set to the converted wire_value
        void field_post_decode(const boost::any& wire_value, boost::any* field_value)
        { any_post_decode(wire_value, field_value); }

        /// \brief Post-decodes a repeated field.
        ///
        /// \param wire_values Should be set to the desired values to translate
        /// \param field_values Will be set to the converted wire_values
        void field_post_decode_repeated(const std::vector<boost::any>& wire_values,
                                        std::vector<boost::any>* field_values)
        { any_post_decode_repeated(wire_values, field_values); }
            
            
        // traverse schema (Descriptor)

        /// \brief Calculate the upper bound on this field's size (in bits)
        ///
        /// \param bit_size Location to <i>add</i> calculated maximum bit size to. Be sure to zero `bit_size` if you want only the size of this field.
        /// \param field Protobuf descriptor to the field. Set to 0 for base message.
        void field_max_size(unsigned* bit_size, const google::protobuf::FieldDescriptor* field);
        /// \brief Calculate the lower bound on this field's size (in bits)
        ///
        /// \param bit_size Location to <i>add</i> calculated minimum bit size to. Be sure to zero `bit_size` if you want only the size of this field.
        /// \param field Protobuf descriptor to the field. Set to 0 for base message.
        void field_min_size(unsigned* bit_size, const google::protobuf::FieldDescriptor* field);

        /// \brief Validate this field, checking that all required option extensions are set (e.g. (goby.field).dccl.max and (goby.field).dccl.min for arithmetic codecs)
        ///
        /// \param b Currently unused (will be set to false)
        /// \param field Protobuf descriptor to the field. Set to 0 for base message.
        /// \throw Exception If field is invalid
        void field_validate(bool* b, const google::protobuf::FieldDescriptor* field);

        /// \brief Write human readable information about the field and its bounds to the provided stream.
        ///
        /// \param os Stream to write info to.
        /// \param field Protobuf descriptor to the field. Set to 0 for base message.
        void field_info(std::ostream* os, const google::protobuf::FieldDescriptor* field);
        //@}
            
      protected:

        /// \brief Get the DCCL field option extension value for the current field
        ///
        /// dccl::DCCLFieldOptions is defined in acomms_option_extensions.proto
        dccl::DCCLFieldOptions dccl_field_options() const 
        {
            if(this_field())
                return this_field()->options().GetExtension(dccl::field);
            else
                throw(Exception("Cannot call dccl_field on base message (has no *field* option extension"));                
                
        }
            
        /// \brief Essentially an assertion to be used in the validate() virtual method
        ///
        /// \param b Boolean to assert (if true, execution continues, if false an exception is thrown)
        /// \param description Debugging description for this assertion (will be appended to the exception)
        /// \throw Exception Thrown if !b
        void require(bool b, const std::string& description)
        {
            if(!b)
            {
                if(this_field())
                    throw(Exception("Field " + this_field()->name() + " failed validation: " + description));
                else
                    throw(Exception("Message " + this_descriptor()->name() + " failed validation: " + description));
            }
                
        }
            
        // 
        // VIRTUAL
        //

        // contain boost::any
        /// \brief Virtual method used to encode
        ///
        /// \param bits Bitset to store encoded bits. Bits is <i>just</i> the bits from the current operation (unlike base_encode() and field_encode() where bits are added to the most significant end).
        /// \param wire_value Value to encode (WireType)
        virtual void any_encode(Bitset* bits, const boost::any& wire_value) = 0;

        /// \brief Virtual method used to decode
        ///
        /// \param bits Bitset containing bits to decode. This will initially contain min_size() bits. If you need more bits, call get_more_bits() with the number of bits required. This bits will be consumed from the bit pool and placed in `bits`.
        /// \param wire_value Place to store decoded value (as FieldType)
        virtual void any_decode(Bitset* bits, boost::any* wire_value) = 0;

        /// \brief Virtual method used to pre-encode (convert from FieldType to WireType). The default implementation of this method is for when WireType == FieldType and simply copies the field_value to the wire_value.
        ///
        /// \param wire_value Converted value (WireType)
        /// \param field_value Value to convert (FieldType)
        virtual void any_pre_encode(boost::any* wire_value,
                                    const boost::any& field_value) 
        { *wire_value = field_value; }

        /// \brief Virtual method used to post-decode (convert from WireType to FieldType). The default implementation of this method is for when WireType == FieldType and simply copies the wire_value to the field_value.
        ///
        /// \param wire_value Value to convert (WireType)
        /// \param field_value Converted value (FieldType)
        virtual void any_post_decode(const boost::any& wire_value,
                                     boost::any* field_value)
        { *field_value = wire_value; }

        /// \brief Virtual method for calculating the size of a field (in bits).
        ///
        /// \param field_value Value to calculate size of
        /// \return Size of field (in bits)
        virtual unsigned any_size(const boost::any& wire_value) = 0;

        // no boost::any
        /// \brief Validate a field. Use require() inside your overloaded validate() to assert requirements or throw Exceptions directly as needed.
        virtual void validate() { }

        /// \brief Write field specific information (in addition to general information such as sizes that are automatically written by this class for all fields.
        ///
        /// \return string containing information to display.
        virtual std::string info();
            
        /// \brief Calculate maximum size of the field in bits
        ///
        /// \return Maximum size of this field (in bits).
        virtual unsigned max_size() = 0;

        /// \brief Calculate minimum size of the field in bits
        ///
        /// \return Minimum size of this field (in bits).
        virtual unsigned min_size() = 0;
            

        virtual void any_encode_repeated(Bitset* bits, const std::vector<boost::any>& wire_values);
        virtual void any_decode_repeated(Bitset* repeated_bits, std::vector<boost::any>* field_values);

        virtual void any_pre_encode_repeated(std::vector<boost::any>* wire_values,
                                             const std::vector<boost::any>& field_values);
            
        virtual void any_post_decode_repeated(const std::vector<boost::any>& wire_values,
                                              std::vector<boost::any>* field_values);
            
        virtual unsigned any_size_repeated(const std::vector<boost::any>& wire_values);
        virtual unsigned max_size_repeated();
        virtual unsigned min_size_repeated();
            
        friend class DCCLFieldCodecManager;
      private:
        // codec information
        void set_name(const std::string& name)
        { name_ = name; }
        void set_field_type(google::protobuf::FieldDescriptor::Type type)
        { field_type_ = type; }
        void set_wire_type(google::protobuf::FieldDescriptor::CppType type)
        { wire_type_ = type; }

        bool variable_size() { return max_size() != min_size(); }            
            
      private:
        static MessageHandler::MessagePart part_;

        static const google::protobuf::Message* root_message_;
            
        std::string name_;
        google::protobuf::FieldDescriptor::Type field_type_;
        google::protobuf::FieldDescriptor::CppType wire_type_;

    };

    inline std::ostream& operator<<(std::ostream& os, const DCCLFieldCodecBase& field_codec )
    {
        using google::protobuf::FieldDescriptor;
        return os << "[FieldCodec '" << field_codec.name() << "']: field type: "
                  << DCCLTypeHelper::find(field_codec.field_type())->as_str()
                  << " (" << DCCLTypeHelper::find(FieldDescriptor::TypeToCppType(field_codec.field_type()))->as_str()
                  << ") | wire type: " << DCCLTypeHelper::find(field_codec.wire_type())->as_str();
    }

    inline Exception type_error(const std::string& action,
                                    const std::type_info& expected,
                                    const std::type_info& got)
    {
        std::string e = "error " + action + ", expected: ";
        e += expected.name();
        e += ", got ";
        e += got.name();
        return Exception(e);
    }

        
}


#endif

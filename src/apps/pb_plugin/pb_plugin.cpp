// Copyright 2015-2019:
//   GobySoft, LLC (2013-)
//   Community contributors (see AUTHORS file)
// File authors:
//   Toby Schneider <toby@gobysoft.org>
//   Stephanie Petillo <stephanie@gobysoft.org>
//   Nathan Knotts <nknotts@gmail.com>
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
#include "gen_units_class_plugin.h"
#include "option_extensions.pb.h"
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>

std::set<std::string> systems_to_include_;
std::set<std::string> base_units_to_include_;
std::string filename_h_;
std::string load_file_cpp_;
std::shared_ptr<std::fstream> load_file_output_;

std::string load_file_base_{
    R"DEL(#include <dccl/codec.h>

// DO NOT REMOVE: required by loader code
std::vector<const google::protobuf::Descriptor*> descriptors;
template<typename PB> struct DCCLLoader { DCCLLoader() { descriptors.push_back(PB::descriptor()); }};
// END: required by loader code


extern "C"
{
    void dccl3_load(dccl::Codec* dccl)
    {
        // DO NOT REMOVE: required by loader code
        for(auto d : descriptors)
            dccl->load(d);
        // END: required by loader code
    }

    void dccl3_unload(dccl::Codec* dccl)
    {
        // DO NOT REMOVE: required by loader code
        for(auto d : descriptors)
            dccl->unload(d);
        // END: required by loader code
    }
}

// BEGIN (TO END OF FILE): AUTOGENERATED LOADERS
)DEL"};

class DCCLGenerator : public google::protobuf::compiler::CodeGenerator
{
  public:
    DCCLGenerator() = default;
    ~DCCLGenerator() override = default;

    // implements CodeGenerator ----------------------------------------
    bool Generate(const google::protobuf::FileDescriptor* file, const std::string& parameter,
                  google::protobuf::compiler::GeneratorContext* generator_context,
                  std::string* error) const override;

  private:
    void generate_message(
        const google::protobuf::Descriptor* desc,
        google::protobuf::compiler::GeneratorContext* generator_context,
        std::shared_ptr<std::string> message_unit_system = std::shared_ptr<std::string>()) const;
    void generate_field(const google::protobuf::FieldDescriptor* field,
                        google::protobuf::io::Printer* printer,
                        std::shared_ptr<std::string> message_unit_system) const;
    bool check_field_type(const google::protobuf::FieldDescriptor* field) const;

    void generate_load_file_headers() const;
    void generate_load_file_message_loader(const google::protobuf::Descriptor* desc) const;
};

bool DCCLGenerator::check_field_type(const google::protobuf::FieldDescriptor* field) const
{
    bool is_integer = field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_INT32 ||
                      field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_INT64 ||
                      field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_UINT32 ||
                      field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_UINT64;

    bool is_float = field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE ||
                    field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_FLOAT;

    if (!is_float && !is_integer)
    {
        throw std::runtime_error("Can only use (dccl.field).base_dimensions on numeric fields");
    }
    return is_integer;
}

bool DCCLGenerator::Generate(const google::protobuf::FileDescriptor* file,
                             const std::string& parameter,
                             google::protobuf::compiler::GeneratorContext* generator_context,
                             std::string* error) const
{
    std::vector<std::pair<std::string, std::string>> options;
    google::protobuf::compiler::ParseGeneratorParameter(parameter, &options);

    for (const auto& option : options)
    {
        const auto& key = option.first;
        const auto& value = option.second;
        if (key == "dccl3_load_file")
        {
            load_file_cpp_ = value;
            load_file_output_ = std::make_shared<std::fstream>(
                load_file_cpp_, std::ios::in | std::ios::out | std::ios::app);

            if (!load_file_output_->is_open())
            {
                *error = "Failed to open dccl3_load_file: " + load_file_cpp_;
                return false;
            }
        }
        else
        {
            *error = "Unknown parameter: " + key;
            return false;
        }
    }

    try
    {
        const std::string& filename = file->name();
        filename_h_ = filename.substr(0, filename.find(".proto")) + ".pb.h";
        //        std::string filename_cc = filename.substr(0, filename.find(".proto")) + ".pb.cc";

        if (load_file_output_)
            generate_load_file_headers();

        for (int message_i = 0, message_n = file->message_type_count(); message_i < message_n;
             ++message_i)
        {
            try
            {
                generate_message(file->message_type(message_i), generator_context);
            }
            catch (std::exception& e)
            {
                throw(
                    std::runtime_error(std::string("Failed to generate DCCL code: \n") + e.what()));
            }
        }

        std::shared_ptr<google::protobuf::io::ZeroCopyOutputStream> include_output(
            generator_context->OpenForInsert(filename_h_, "includes"));
        google::protobuf::io::Printer include_printer(include_output.get(), '$');
        std::stringstream includes_ss;

        includes_ss << "#include <boost/units/quantity.hpp>" << std::endl;
        includes_ss << "#include <boost/units/absolute.hpp>" << std::endl;
        includes_ss << "#include <boost/units/dimensionless_type.hpp>" << std::endl;
        includes_ss << "#include <boost/units/make_scaled_unit.hpp>" << std::endl;

        for (const auto& it : systems_to_include_) { include_units_headers(it, includes_ss); }
        for (const auto& it : base_units_to_include_)
        { include_base_unit_headers(it, includes_ss); }
        include_printer.Print(includes_ss.str().c_str());

        return true;
    }
    catch (std::exception& e)
    {
        *error = e.what();
        return false;
    }
}

void DCCLGenerator::generate_message(
    const google::protobuf::Descriptor* desc,
    google::protobuf::compiler::GeneratorContext* generator_context,
    std::shared_ptr<std::string> message_unit_system) const
{
    try
    {
        {
            std::shared_ptr<google::protobuf::io::ZeroCopyOutputStream> output(
                generator_context->OpenForInsert(filename_h_, "class_scope:" + desc->full_name()));
            google::protobuf::io::Printer printer(output.get(), '$');

            if (desc->options().HasExtension(dccl::msg))
            {
                if (desc->options().GetExtension(dccl::msg).id() != 0)
                {
                    std::stringstream id_enum;
                    id_enum << "enum DCCLParameters { DCCL_ID = "
                            << desc->options().GetExtension(dccl::msg).id() << ", "
                            << " DCCL_MAX_BYTES = "
                            << desc->options().GetExtension(dccl::msg).max_bytes() << " };\n";
                    printer.Print(id_enum.str().c_str());

                    if (load_file_output_)
                        generate_load_file_message_loader(desc);
                }

                // set message level unit system - used if fields do not specify
                const dccl::DCCLMessageOptions& dccl_msg_options =
                    desc->options().GetExtension(dccl::msg);
                if (dccl_msg_options.has_unit_system())
                {
                    message_unit_system.reset(new std::string(dccl_msg_options.unit_system()));
                    systems_to_include_.insert(dccl_msg_options.unit_system());
                }
            }

            for (int field_i = 0, field_n = desc->field_count(); field_i < field_n; ++field_i)
            { generate_field(desc->field(field_i), &printer, message_unit_system); }

            for (int nested_type_i = 0, nested_type_n = desc->nested_type_count();
                 nested_type_i < nested_type_n; ++nested_type_i)
                generate_message(desc->nested_type(nested_type_i), generator_context,
                                 message_unit_system);
        }
    }
    catch (std::exception& e)
    {
        throw(std::runtime_error(std::string("Message: \n" + desc->full_name() + "\n" + e.what())));
    }
}

void DCCLGenerator::generate_field(const google::protobuf::FieldDescriptor* field,
                                   google::protobuf::io::Printer* printer,
                                   std::shared_ptr<std::string> message_unit_system) const
{
    try
    {
        const dccl::DCCLFieldOptions& dccl_field_options =
            field->options().GetExtension(dccl::field);

        if (!dccl_field_options.has_units())
        {
            return;
        }

        if ((dccl_field_options.units().has_base_dimensions() &&
             dccl_field_options.units().has_derived_dimensions()) ||
            (dccl_field_options.units().has_base_dimensions() &&
             dccl_field_options.units().has_unit()) ||
            (dccl_field_options.units().has_unit() &&
             dccl_field_options.units().has_derived_dimensions()))
        {
            throw(std::runtime_error("May define either (dccl.field).units.base_dimensions or "
                                     "(dccl.field).units.derived_dimensions or "
                                     "(dccl.field).units.unit, but not more than one."));
        }
        else if (dccl_field_options.units().has_unit())
        {
            std::stringstream new_methods;

            construct_units_typedef_from_base_unit(
                field->name(), dccl_field_options.units().unit(),
                dccl_field_options.units().relative_temperature(),
                dccl_field_options.units().prefix(), new_methods);
            construct_field_class_plugin(field->name(), new_methods,
                                         dccl::units::get_field_type_name(field->cpp_type()),
                                         field->is_repeated());
            printer->Print(new_methods.str().c_str());
            base_units_to_include_.insert(dccl_field_options.units().unit());
        }
        else if (dccl_field_options.units().has_base_dimensions())
        {
            std::stringstream new_methods;

            std::vector<double> powers;
            std::vector<std::string> short_dimensions;
            std::vector<std::string> dimensions;
            if (dccl::units::parse_base_dimensions(
                    dccl_field_options.units().base_dimensions().begin(),
                    dccl_field_options.units().base_dimensions().end(), powers, short_dimensions,
                    dimensions))
            {
                if (!dccl_field_options.units().has_system() && !message_unit_system)
                    throw(std::runtime_error(
                        std::string("Field must have 'system' defined or message must have "
                                    "'unit_system' defined when using 'base_dimensions'.")));

                // default to system set in the field, otherwise use the system set at the message level
                const std::string& unit_system =
                    (!dccl_field_options.units().has_system() && message_unit_system)
                        ? *message_unit_system
                        : dccl_field_options.units().system();

                construct_base_dims_typedef(dimensions, powers, field->name(), unit_system,
                                            dccl_field_options.units().relative_temperature(),
                                            dccl_field_options.units().prefix(), new_methods);

                construct_field_class_plugin(field->name(), new_methods,
                                             dccl::units::get_field_type_name(field->cpp_type()),
                                             field->is_repeated());
                printer->Print(new_methods.str().c_str());
                systems_to_include_.insert(unit_system);
            }
            else
            {
                throw(std::runtime_error(std::string("Failed to parse base_dimensions string: \"" +
                                                     dccl_field_options.units().base_dimensions() +
                                                     "\"")));
            }
        }
        else if (dccl_field_options.units().has_derived_dimensions())
        {
            std::stringstream new_methods;

            std::vector<std::string> operators;
            std::vector<std::string> dimensions;
            if (dccl::units::parse_derived_dimensions(
                    dccl_field_options.units().derived_dimensions().begin(),
                    dccl_field_options.units().derived_dimensions().end(), operators, dimensions))
            {
                if (!dccl_field_options.units().has_system() && !message_unit_system)
                    throw(std::runtime_error(
                        std::string("Field must have 'system' defined or message must have "
                                    "'unit_system' defined when using 'derived_dimensions'.")));
                const std::string& unit_system =
                    (!dccl_field_options.units().has_system() && message_unit_system)
                        ? *message_unit_system
                        : dccl_field_options.units().system();

                construct_derived_dims_typedef(dimensions, operators, field->name(), unit_system,
                                               dccl_field_options.units().relative_temperature(),
                                               dccl_field_options.units().prefix(), new_methods);

                construct_field_class_plugin(field->name(), new_methods,
                                             dccl::units::get_field_type_name(field->cpp_type()),
                                             field->is_repeated());
                printer->Print(new_methods.str().c_str());
                systems_to_include_.insert(unit_system);
            }
            else
            {
                throw(std::runtime_error(
                    std::string("Failed to parse derived_dimensions string: \"" +
                                dccl_field_options.units().derived_dimensions() + "\"")));
            }
        }
    }
    catch (std::exception& e)
    {
        throw(
            std::runtime_error(std::string("Field: \n" + field->DebugString() + "\n" + e.what())));
    }
}

void DCCLGenerator::generate_load_file_headers() const
{
    bool file_is_empty = (load_file_output_->peek() == std::ifstream::traits_type::eof());
    load_file_output_->clear();

    bool header_already_written = false;
    if (file_is_empty)
    {
        *load_file_output_ << load_file_base_ << std::flush;
    }
    else
    {
        for (std::string line; getline(*load_file_output_, line);)
        {
            if (line.find("\"" + filename_h_ + "\"") != std::string::npos)
            {
                header_already_written = true;
                break;
            }
        }
    }
    // clear EOF
    load_file_output_->clear();
    // seek to the end
    load_file_output_->seekp(0, std::ios_base::end);

    if (!header_already_written)
        *load_file_output_ << "#include \"" << filename_h_ << "\"" << std::endl;
}

void DCCLGenerator::generate_load_file_message_loader(
    const google::protobuf::Descriptor* desc) const
{
    // seek to the beginning
    load_file_output_->seekp(0, std::ios_base::beg);

    // cpp class name
    std::string cpp_name = desc->full_name();
    boost::algorithm::replace_all(cpp_name, ".", "::");

    // lower case class name with underscores
    std::string loader_name = desc->full_name() + "_loader";
    boost::algorithm::replace_all(loader_name, ".", "_");
    boost::algorithm::to_lower(loader_name);

    bool loader_already_written = false;
    for (std::string line; getline(*load_file_output_, line);)
    {
        if (line.find("<" + cpp_name + ">") != std::string::npos)
        {
            loader_already_written = true;
            break;
        }
    }
    load_file_output_->clear();
    load_file_output_->seekp(0, std::ios_base::end);

    if (!loader_already_written)
        *load_file_output_ << "DCCLLoader<" << cpp_name << "> " << loader_name << ";" << std::endl;
}

int main(int argc, char* argv[])
{
    DCCLGenerator generator;
    return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}

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


#include <string>

#include "MOOSLIB/MOOSMsg.h"

namespace goby
{
    namespace moos
    {
 
        class MOOSSerializer
        {
          public:
            static void serialize(const CMOOSMsg& const_msg, std::string* data)
            {
                // copy because Serialize wants to modify the CMOOSMsg
                CMOOSMsg msg(const_msg);
                // adapted from MOOSCommPkt.cpp
                const unsigned PKT_TMP_BUFFER_SIZE = 40000;
                int serialized_size = PKT_TMP_BUFFER_SIZE;
                data->resize(serialized_size);
                serialized_size = msg.Serialize(reinterpret_cast<unsigned char*>(&(*data)[0]), serialized_size);
                data->resize(serialized_size);
            }
            
            static void parse(CMOOSMsg* msg, std::string data)
            {
                msg->Serialize(reinterpret_cast<unsigned char*>(&data[0]), data.size(), false);
            }
            
        };
    }
}
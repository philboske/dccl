// copyright 2008, 2009 t. schneider tes@mit.edu
// 
// this file is part of the Dynamic Compact Control Language (DCCL),
// the goby-acomms codec. goby-acomms is a collection of libraries 
// for acoustic underwater networking
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this software.  If not, see <http://www.gnu.org/licenses/>.

#ifndef MESSAGE_ALGORITHMS20091211H
#define MESSAGE_ALGORITHMS20091211H

#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <cctype>

#include <boost/algorithm/string.hpp>
#include <boost/function.hpp>
namespace goby
{
    namespace transitional
    {
        class DCCLMessageVal;
        class DCCLMessage;
    
        /// \brief boost::function for a function taking a single DCCLMessageVal reference. Used for algorithm callbacks.
        ///
        /// Think of this as a generalized version of a function pointer (void (*)(DCCLMessageVal&)). See http://www.boost.org/doc/libs/1_34_0/doc/html/function.html for more on boost:function.
        typedef boost::function<void (DCCLMessageVal&)> AlgFunction1;
        /// \brief boost::function for a function taking a dccl::MessageVal reference, and the MessageVal of a second part of the message. Used for algorithm callbacks.
        ///
        /// Think of this as a generalized version of a function pointer (void (*)(DCCLMessageVal&, const DCCLMessageVal&). See http://www.boost.org/doc/libs/1_34_0/doc/html/function.html for more on boost:function.
        typedef boost::function<void (DCCLMessageVal&, const std::vector<DCCLMessageVal>&)> AlgFunction2;

        class DCCLAlgorithmPerformer
        {
          public:
            static DCCLAlgorithmPerformer* getInstance();
            static void deleteInstance();

            void algorithm(DCCLMessageVal& in,
                           unsigned array_index,
                           const std::string& algorithm,
                           const std::map<std::string,std::vector<DCCLMessageVal> >& vals);

            void run_algorithm(const std::string& algorithm,
                               DCCLMessageVal& in,
                               const std::vector<DCCLMessageVal>& ref);
            
            
            void add_algorithm(const std::string& name, AlgFunction1 func)
            { adv_map1_[name] = func; }

            void add_adv_algorithm(const std::string& name, AlgFunction2 func)
            { adv_map2_[name] = func; }

            void check_algorithm(const std::string& alg, const DCCLMessage& msg);

          private:
            static DCCLAlgorithmPerformer* inst_;
            std::map<std::string, AlgFunction1> adv_map1_;
            std::map<std::string, AlgFunction2> adv_map2_;
        
            DCCLAlgorithmPerformer()
            {}
        
            DCCLAlgorithmPerformer(const DCCLAlgorithmPerformer&);
            DCCLAlgorithmPerformer& operator = (const DCCLAlgorithmPerformer&);

            
        };
    }
}


#endif

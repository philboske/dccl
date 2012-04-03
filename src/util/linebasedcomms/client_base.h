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


#ifndef ClientBase20100628H
#define ClientBase20100628H

#include "interface.h"
#include "connection.h"

namespace goby
{
    namespace util
    {
        // template for type of client socket (asio::serial_port, asio::ip::tcp::socket)
        // LineBasedInterface runs in a one thread (same as user program)
        // LineBasedClient and LineBasedConnection run in another thread (spawned by LineBasedInterface's constructor)
        template<typename ASIOAsyncReadStream>
            class LineBasedClient : public LineBasedInterface, public LineBasedConnection<ASIOAsyncReadStream>
        {
            
          protected:
          LineBasedClient(const std::string& delimiter)
              : LineBasedInterface(delimiter),
                LineBasedConnection<ASIOAsyncReadStream>(this)
                { }            

            enum { RETRY_INTERVAL = 10 };            

            virtual ASIOAsyncReadStream& socket () = 0;
            virtual std::string local_endpoint() = 0;
            virtual std::string remote_endpoint() = 0;
            
            // from LineBasedInterface
            void do_start()                     
            {
                last_start_time_ = common::goby_time();
                
                set_active(start_specific());
                
                LineBasedConnection<ASIOAsyncReadStream>::read_start();
                if(!LineBasedConnection<ASIOAsyncReadStream>::out().empty())
                    LineBasedConnection<ASIOAsyncReadStream>::write_start();
            }
            
            virtual bool start_specific() = 0;

            // from LineBasedInterface
            void do_write(const protobuf::Datagram& line)
            { 
                bool write_in_progress = !LineBasedConnection<ASIOAsyncReadStream>::out().empty();
                LineBasedConnection<ASIOAsyncReadStream>::out().push_back(line);
                if (!write_in_progress) LineBasedConnection<ASIOAsyncReadStream>::write_start();
            }

            // from LineBasedInterface
            void do_close(const boost::system::error_code& error)
            { 
                if (error == boost::asio::error::operation_aborted) // if this call is the result of a timer cancel()
                    return; // ignore it because the connection cancelled the timer
                
                set_active(false);
                socket().close();
                
                using namespace boost::posix_time;
                ptime now  = common::goby_time();
                if(now - seconds(RETRY_INTERVAL) < last_start_time_)
                    sleep(RETRY_INTERVAL - (now-last_start_time_).total_seconds());
                
                do_start();
            }

            // same as do_close in this case
            void socket_close(const boost::system::error_code& error)
            { do_close(error); }

          private:        
            boost::posix_time::ptime last_start_time_;
           
            LineBasedClient(const LineBasedClient&);
            LineBasedClient& operator= (const LineBasedClient&);
        
        }; 

    }
}


#endif
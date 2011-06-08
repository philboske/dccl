// copyright 2010 t. schneider tes@mit.edu
// 
// this file is part of goby_dbo
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

#include <stdexcept>
#include <algorithm>

#include <Wt/Dbo/backend/Sqlite3>

#include "goby/util/string.h"
#include "goby/util/time.h"
#include "goby/util/logger.h"
#include "wt_dbo_overloads.h"
#include "goby/core/libdbo/dbo_manager.h"

#include "dbo_plugin.h"


goby::core::DBOManager* goby::core::DBOManager::inst_ = 0;

using goby::util::goby_time;
using goby::glog;


// singleton class, use this to get pointer
goby::core::DBOManager* goby::core::DBOManager::get_instance()
{
    if(!inst_) inst_ = new goby::core::DBOManager();
    return(inst_);
}

void goby::core::DBOManager::shutdown()
{
    if(inst_) delete inst_;
}


goby::core::DBOManager::DBOManager()
       :connection_(0),
        session_(0),
        transaction_(0),
        t_last_commit_(goby_time()),
        static_tables_created_(false)
{
    glog.add_group("dbo", goby::util::Colors::lt_green, "database");
}

goby::core::DBOManager::~DBOManager()
{
    if(transaction_) delete transaction_;
    if(connection_) delete connection_;
    if(session_) delete session_;
}


void goby::core::DBOManager::add_raw(MarshallingScheme marshalling_scheme,
                                     const std::string& identifier,
                                     const void* data,
                                     int size,
                                     int socket_id)
{
    static int i = -1;
    ++i;
    
    RawEntry *new_entry = new RawEntry();
    new_entry->marshalling_scheme = marshalling_scheme;
    new_entry->identifier = identifier;
    new_entry->unique_id = i;
    new_entry->socket_id = socket_id;

    std::string bytes = std::string(static_cast<const char*>(data), size);
    new_entry->bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());
    new_entry->time = goby::util::as<std::string>(goby::util::goby_time());
                
    session_->add(new_entry);

    DBOPlugin* plugin = plugin_factory_.plugin(marshalling_scheme);
    if(plugin)
        plugin->add_message(i, identifier, data, size);
}

void goby::core::DBOManager::commit()
{
    glog.is(verbose) &&
        glog << group("dbo") << "starting commit" << std::endl;
        
    transaction_->commit();
        
    glog.is(verbose) &&
        glog << group("dbo") << "finished commit" << std::endl;

    t_last_commit_ = goby_time();

    delete transaction_;
    transaction_ = new Wt::Dbo::Transaction(*session_);
}



void goby::core::DBOManager::connect(const std::string& db_name /* = "" */)
{
    if(!db_name.empty())
        db_name_ = db_name;

    if(transaction_) delete transaction_;
    if(connection_) delete connection_;
    if(session_) delete session_;
    connection_ = new Wt::Dbo::backend::Sqlite3(db_name_);
    session_ = new Wt::Dbo::Session;
    session_->setConnection(*connection_);
    // transaction deleted by session
    transaction_ = new Wt::Dbo::Transaction(*session_);
    
    if(!static_tables_created_)
    {
        map_types();
        try{ session_->createTables(); }
        catch(Wt::Dbo::Exception& e)
        {
            glog.is(warn) &&
                glog << e.what() << std::endl;
        }

        glog.is(verbose) &&
            glog << group("dbo") << "created table for static types" << std::endl;
        static_tables_created_ = true;
//        reset_session();
    }
    
    
}

void goby::core::DBOManager::reset_session()
{
    commit();
    glog.is(verbose) &&
        glog << "resetting session" << std::endl;
    connect();
}
            

void goby::core::DBOManager::map_types()
{
    session_->mapClass<RawEntry>("_raw");

    std::set<DBOPlugin*> plugins = plugin_factory_.plugins();
    BOOST_FOREACH(DBOPlugin* plugin, plugins)    
    {
        if(plugin)
            plugin->map_types();
    }
}

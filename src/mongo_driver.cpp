#include "mongo_driver.hpp"
#include "bsoncxx/builder/basic/document.hpp"
#include "bsoncxx/builder/basic/kvp.hpp"
#include "bsoncxx/document/view.hpp"
#include "bsoncxx/json.hpp"
#include <exception>
#include <string>
#include <utility>
#define DB_NAME "iptv"

static mongocxx::instance inst{};
static mongocxx::client client{mongocxx::uri{}};

using bsoncxx::builder::basic::make_document;
using bsoncxx::builder::basic::kvp;

void Mongo::fill_defauls(){
    try{
    }catch(std::exception& e){
        std::cout << e.what() << '\n';
    }
}
void Mongo::info()
{
    BOOST_LOG_TRIVIAL(trace) << __func__ ;
    auto dbs = client.database("iptv");
    auto cols = dbs.list_collection_names();
    for(auto col : cols){
        std::cout << "col:" << col << '\n';
    }
}
bool Mongo::exists(std::string col_name, std::string doc)
{
    auto db = client[DB_NAME];     
    
    BOOST_LOG_TRIVIAL(trace) << __func__ ;
    auto result = db[col_name].count_documents(bsoncxx::from_json(doc));
    if(result > 0) return true;
    return false;
}
bool Mongo::exists_id(std::string col_name, int id)
{
    BOOST_LOG_TRIVIAL(trace) << __func__ << " id:" << id;
    auto db = client[DB_NAME];     
    
    auto result = db[col_name].count_documents(make_document(kvp("_id", id)));
    if(result > 0) return true;
    return false;
}
bool Mongo::insert(std::string col, std::string doc)
{
    BOOST_LOG_TRIVIAL(trace) << __func__ ;
    try{
        auto dB = client[DB_NAME];
        auto ret = dB[col].insert_one(bsoncxx::from_json(doc));
        return ret.value().inserted_id().get_int64() >= 0;
    }catch(...){
        return false;
    }
}
bool Mongo::remove(std::string col, std::string doc)
{
    BOOST_LOG_TRIVIAL(trace) << __func__ ;
    try{
        auto dB = client[DB_NAME];
        auto ret = dB[col].delete_one(bsoncxx::from_json(doc));
        return ret.value().deleted_count() > 0;
    }catch(...){
        return false;
    }

}
bool Mongo::remove_by_id(std::string col, int id)
{
    BOOST_LOG_TRIVIAL(trace) << __func__ << " id:" << id;
    try{
        auto dB = client[DB_NAME];
        auto ret = dB[col].delete_one(make_document(kvp("_id", id)));
        return ret.value().deleted_count() > 0;
    }catch(...){
        return false;
    }

}
bool Mongo::replace(std::string col, std::string filter, std::string doc)
{
    BOOST_LOG_TRIVIAL(trace) << __func__ ;
    try{
        auto dB = client[DB_NAME];
        auto ret = dB[col].replace_one(bsoncxx::from_json(filter) ,bsoncxx::from_json(doc));
        return ret.value().modified_count() > 0;
    }catch(...){
        return false;
    }
    return false;

}
bool Mongo::replace_by_id(std::string col, int id, std::string doc)
{
    BOOST_LOG_TRIVIAL(trace) << __func__ << " id:" << id;
    try{
        auto dB = client[DB_NAME];
        auto ret = dB[col].replace_one(make_document(kvp("_id", id)) ,
                                       bsoncxx::from_json(doc));
        return ret.value().modified_count() > 0;
    }catch(...){
        return false;
    }
    return false;

}
std::string Mongo::find( std::string col, std::string doc)
{
    auto dB = client[DB_NAME];     
    
    BOOST_LOG_TRIVIAL(trace) <<  __func__;

    auto result = dB[col].find(bsoncxx::from_json(doc));
    std::string result_str = "";
    int count = 0;
    for(auto&& e : result){
        count++;
        result_str += bsoncxx::to_json(e) + ","; 
    }
    if(result_str.size() == 0) return "";
    result_str.pop_back();
    if(count > 1){
        result_str = "[ " + result_str + " ]";
    }
    return result_str;
}
std::string Mongo::find_id(std::string col, int id)
{
    auto dB = client[DB_NAME];     
   
    BOOST_LOG_TRIVIAL(trace) << __func__ << " id:" << id;
    try{
        bsoncxx::document::view  v;
        auto result = dB[col].find(make_document(kvp("_id", id)));
        std::string result_str = "";
        for(auto e : result){
                result_str = bsoncxx::to_json(e); 
        }
        return result_str;

    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(trace) << "Exception:" << e.what() ;
        return "";
    }
}
std::string Mongo::find_id_range(std::string col, int begin, int end)
{
    auto dB = client[DB_NAME];     
   
    BOOST_LOG_TRIVIAL(trace) << __func__ << " begin:" << begin << " end:" << end;
    try{
        bsoncxx::document::view  v;
        auto result = dB[col].find(v);
        std::string result_str = "";
        int total = 0;
        for(auto e : result){
            if(e.find("_id") == e.end()){
                BOOST_LOG_TRIVIAL(trace) << " _id not found!" ;
                continue;
            }
            int id = e["_id"].get_int32();
            total++;
            if(id >= begin && id <= end){
                result_str += bsoncxx::to_json(e) + ","; 
            }
        }
        if(result_str.size() > 0)
            result_str.pop_back();
        result_str =  "{ total: " + std::to_string(total) + 
                         ", content:[" + result_str + "] }";
        return result_str;

    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(trace) << "Exception:" << e.what() ;
        return "";
    }
}
std::string Mongo::find_time_id_range(std::string col, 
        long stime, long etime, int begin, int end)
{
    auto dB = client[DB_NAME];     
   
    BOOST_LOG_TRIVIAL(trace) << __func__ << " begin:" << begin << " end:" << end;
    try{
        auto result = dB[col].find(
                make_document(kvp("time", 
                       make_document(kvp("$gt", stime), kvp("$lt", etime))
                        )));
        std::string result_str = "";
        int total = 0;
        for(auto e : result){
            if(e.find("_id") == e.end()){
                BOOST_LOG_TRIVIAL(trace) << " _id not found!" ;
                continue;
            }
            int id = e["_id"].get_int32();
            total++;
            if(id >= begin && id <= end){
                result_str += bsoncxx::to_json(e) + ","; 
            }
        }
        if(result_str.size() > 0)
            result_str.pop_back();
        result_str =  "{ total: " + std::to_string(total) + 
                         ", content:[" + result_str + "] }";
        return result_str;

    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(trace) << "Exception:" << e.what() ;
        return "";
    }
}

#include <exception>
#include <string>
#include <utility>
#include <boost/log/trivial.hpp>
#include <mongocxx/logger.hpp>
#include <mongocxx/options/client.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/stdx/make_unique.hpp>
#include "mongo_driver.hpp"

using bsoncxx::builder::basic::make_document;
using bsoncxx::builder::basic::kvp;
static mongocxx::instance inst{};

void Mongo::fill_defauls(){
    try{
        if(!exists_id("uniq_counter", 1)){    
            insert("uniq_counter", "{ \"_id\":1, \"count\":100000 }");
        }
    }catch(std::exception& e){
        std::cout << e.what() << '\n';
    }
}
void Mongo::info()
{
    BOOST_LOG_TRIVIAL(debug) << __func__ ;
    std::lock_guard<std::mutex> lo(db_mutex);
    auto dbs = client.database("iptv");
    auto cols = dbs.list_collection_names();
    for(auto col : cols){
        std::cout << "col:" << col << '\n';
    }
}
bool Mongo::exists(const std::string col_name, const std::string doc)
{
    BOOST_LOG_TRIVIAL(debug) << __func__  << col_name << doc;
    std::lock_guard<std::mutex> lo(db_mutex);
    auto result = db[col_name].count_documents(bsoncxx::from_json(doc));
    if(result > 0) return true;
    return false;
}
bool Mongo::exists_id(const std::string col_name, int id)
{
    BOOST_LOG_TRIVIAL(debug) << __func__ << " col:"<< col_name << " id:" << id;
    std::lock_guard<std::mutex> lo(db_mutex);
    auto result = db[col_name].count_documents(make_document(kvp("_id", id)));
    if(result > 0) return true;
    return false;
}
bool Mongo::insert(const std::string col, const std::string doc)
{
    BOOST_LOG_TRIVIAL(debug) << __func__  << " in col:" << col << " doc:" << doc;
    std::lock_guard<std::mutex> lo(db_mutex);
    try{
        auto ret = db[col].insert_one(bsoncxx::from_json(doc));
        return ret.value().inserted_id().get_int64() >= 0;
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
        return false;
    }
}
bool Mongo::remove_mony(const std::string col, const std::string doc)
{
    BOOST_LOG_TRIVIAL(debug) << __func__  << " from col:" << col << " doc:" << doc;
    std::lock_guard<std::mutex> lo(db_mutex);
    try{
        auto ret = db[col].delete_many(bsoncxx::from_json(doc));
        return ret.value().deleted_count() > 0;
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
        return false;
    }
}
bool Mongo::remove_id(const std::string col, int id)
{
    BOOST_LOG_TRIVIAL(debug) << __func__  << " from col:" << col << " id:" << id;
    std::lock_guard<std::mutex> lo(db_mutex);
    try{
        auto ret = db[col].delete_one(make_document(kvp("_id", id)));
        return ret.value().deleted_count() > 0;
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
        return false;
    }
}
bool Mongo::replace(const std::string col, const std::string filter, const std::string doc)
{
    BOOST_LOG_TRIVIAL(debug) << __func__  << " in col:" << col << " filter:" << filter;
    std::lock_guard<std::mutex> lo(db_mutex);
    try{
        auto ret = db[col].replace_one(bsoncxx::from_json(filter) ,
                bsoncxx::from_json(doc));
        return ret.value().modified_count() > 0;
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
        return false;
    }
}
bool Mongo::insert_or_replace_id(const std::string col, int id, 
        const std::string doc)
{
    BOOST_LOG_TRIVIAL(debug) << __func__  << " in col:" << col << " doc:" << doc;
    std::lock_guard<std::mutex> lo(db_mutex);
    try{
        mongocxx::options::replace options {};
        options.upsert(true);
        auto ret = db[col].replace_one(make_document(kvp("_id", id)) ,
                bsoncxx::from_json(doc),
                options);
        return ret.value().modified_count() > 0;
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception:" << e.what() ;
    }
    return false;
}
bool Mongo::replace_id(const std::string col, int id, const std::string doc)
{
    BOOST_LOG_TRIVIAL(debug) << __func__ << " id:" << id;
    std::lock_guard<std::mutex> lo(db_mutex);
    try{
        auto ret = db[col].replace_one(make_document(kvp("_id", id)) ,
                bsoncxx::from_json(doc));
        return ret.value().modified_count() > 0;
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
        return false;
    }
    return false;
}
const std::string Mongo::find_mony( const std::string col, const std::string doc)
{
    BOOST_LOG_TRIVIAL(debug) << __func__  << " in col:" << col << " doc:" << doc;
    std::lock_guard<std::mutex> lo(db_mutex);
    try{
        auto result = db[col].find(bsoncxx::from_json(doc));
        std::string result_str;
        for(const auto& e : result){
            result_str += bsoncxx::to_json(e) + ","; 
        }
        if(result_str.size() > 0) result_str.pop_back();
        return "[" + result_str + "]";
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
    }
    return "[]";
}
const std::string Mongo:: find_one( const std::string col, const std::string doc)
{
    BOOST_LOG_TRIVIAL(debug) << __func__  << " in col:" << col << " doc:" << doc;
    std::lock_guard<std::mutex> lo(db_mutex);
    try{
        auto result = db[col].find_one(bsoncxx::from_json(doc));
        if (result && !(result->view().empty())) {
            bsoncxx::document::view  v(result->view());
            return bsoncxx::to_json(v); 
        }
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
    }
    return "{}";
}
const std::string Mongo:: find_id(const std::string col, int id)
{
    BOOST_LOG_TRIVIAL(debug) << __func__  << " in col:" << col << " id:" << id;
    std::lock_guard<std::mutex> lo(db_mutex);
    try{
        auto result = db[col].find_one(make_document(kvp("_id", id)));
        if (result && !(result->view().empty())) {
            bsoncxx::document::view  v(result->view());
            return bsoncxx::to_json(v); 
        }
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
    }
    return "{}";
}
int Mongo::get_uniq_id()
{
    BOOST_LOG_TRIVIAL(debug) << __func__; 
    std::lock_guard<std::mutex> lo(db_mutex);
    std::string col = "uniq_counter"; 
    int id = 1;
    try{
        mongocxx::options::find_one_and_update options {};
        options.return_document(mongocxx::options::return_document::k_after);
        auto result = db[col].find_one_and_update(
                make_document(kvp("_id", id)),
                make_document(kvp("$inc", make_document(kvp("count", 1)))),
                options);
        if (result && !(result->view().empty())) {
            bsoncxx::document::view  v(result->view());
            int newId = v["count"].get_int32(); 
            return newId; 
        }
        return 1; 
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
        return 1;
    }
}
const std::string Mongo:: find_range(const std::string col, int begin, int end)
{
    BOOST_LOG_TRIVIAL(debug) << __func__  << " in col:" << col << 
        " begin:" << begin << " end:" << end;
    std::lock_guard<std::mutex> lo(db_mutex);
    try{
        mongocxx::options::find options {};
        options.skip(begin);
        options.limit(end - begin + 1);
        bsoncxx::document::view  v;
        int total = db[col].count_documents(v);
        auto result = db[col].find(v, options);
        std::string result_str = "";
        for(auto e : result){
            result_str += bsoncxx::to_json(e) + ","; 
        }
        if(result_str.size() > 0) result_str.pop_back();
        result_str =  "{ \"total\": " + std::to_string(total) + 
            ", \"content\":[" + result_str + "] }";
        return result_str;
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
        return "{ \"total\": 0, \"content\":[] }";
    }
}
const std::string Mongo:: find_filter_range(const std::string col, 
        const std::string filter,
        int begin, int end)
{
    BOOST_LOG_TRIVIAL(debug) << __func__ << " begin:" << begin << " end:" << end
        << " col:" << col <<" filter:" << filter;
    std::lock_guard<std::mutex> lo(db_mutex);
    try{
        mongocxx::options::find options {};
        options.skip(begin);
        options.limit(end - begin + 1);
        auto result = db[col].find( bsoncxx::from_json(filter),options);
        std::string result_str = "";
        for(auto e : result){
            result_str += bsoncxx::to_json(e) + ","; 
        }
        if(result_str.size() > 0)
            result_str.pop_back();
        int total = db[col].count_documents(bsoncxx::from_json(filter));
        return "{ \"total\": " + std::to_string(total) + 
            ", \"content\":[" + result_str + "] }";
    }catch(std::exception& e){
        BOOST_LOG_TRIVIAL(error) << "Exception in " << __func__ << ":" << e.what();
        return "{ \"total\": 0, \"content\":[] }";
    }
}


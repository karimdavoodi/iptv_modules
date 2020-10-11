/*
 * Copyright (c) 2020 Karim, karimdavoodi@gmail.com
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
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
#include "config.hpp"

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
    auto dbs = client.database("iptv");
    auto cols = dbs.list_collection_names();
    for(auto col : cols){
        std::cout << "col:" << col << '\n';
    }
}
bool Mongo::exists(const std::string col_name, const std::string doc)
{
    LOG(trace) << col_name << doc;
    auto result = db[col_name].count_documents(bsoncxx::from_json(doc));
    if(result > 0) return true;
    return false;
}
bool Mongo::exists_id(const std::string col_name, int64_t id)
{
    LOG(trace) << " col:"<< col_name << " id:" << id;
    auto result = db[col_name].count_documents(make_document(kvp("_id", id)));
    if(result > 0) return true;
    return false;
}
long Mongo::count(const std::string col, const std::string filter)
{
    LOG(trace) << "col:" << col; 
    try{
        return db[col].count_documents(bsoncxx::from_json(filter));
    }catch(std::exception& e){
        LOG(error) << e.what();
        return 0;
    }
}
bool Mongo::insert(const std::string col, const std::string doc)
{
    LOG(info) << " in col:" << col << " doc:" << doc;
    try{
        auto ret = db[col].insert_one(bsoncxx::from_json(doc));
        return ret.value().inserted_id().get_int64() >= 0;
    }catch(std::exception& e){
        LOG(error) << e.what();
        return false;
    }
}
bool Mongo::remove_mony(const std::string col, const std::string doc)
{
    LOG(info) << " from col:" << col << " doc:" << doc;
    try{
        auto ret = db[col].delete_many(bsoncxx::from_json(doc));
        return ret.value().deleted_count() > 0;
    }catch(std::exception& e){
        LOG(error) << e.what();
        return false;
    }
}
bool Mongo::remove_id(const std::string col, int64_t id)
{
    LOG(info) << " from col:" << col << " id:" << id;
    try{
        auto ret = db[col].delete_one(make_document(kvp("_id", id)));
        return ret.value().deleted_count() > 0;
    }catch(std::exception& e){
        LOG(error) << e.what();
        return false;
    }
}
bool Mongo::replace(const std::string col, const std::string filter, const std::string doc)
{
    LOG(info) << "col:" << col << " filter:" << filter;
    try{
        auto ret = db[col].replace_one(bsoncxx::from_json(filter) ,
                bsoncxx::from_json(doc));
        return ret.value().modified_count() > 0;
    }catch(std::exception& e){
        LOG(error) << e.what();
        return false;
    }
}
bool Mongo::insert_or_replace_filter(const std::string col, const std::string filter,  
        const std::string doc)
{
    LOG(trace) << "doc:" << doc;
    try{
        mongocxx::options::replace options {};
        options.upsert(true);
        auto ret = db[col].replace_one(bsoncxx::from_json(filter),
                bsoncxx::from_json(doc),
                options);
        return ret.value().modified_count() > 0;
    }catch(std::exception& e){
        LOG(error) << e.what() ;
    }
    return false;
}
bool Mongo::insert_or_replace_id(const std::string col, int64_t id, 
        const std::string doc)
{
    LOG(info) << "col:" << col << " id:" << id;
    LOG(trace) << "doc:" << doc;
    try{
        mongocxx::options::replace options {};
        options.upsert(true);
        auto ret = db[col].replace_one(make_document(kvp("_id", id)) ,
                bsoncxx::from_json(doc),
                options);
        return ret.value().modified_count() > 0;
    }catch(std::exception& e){
        LOG(error) << e.what() ;
    }
    return false;
}
bool Mongo::replace_id(const std::string col, int64_t id, const std::string doc)
{
    LOG(info) << "col:" << col << " id:" << id;
    try{
        auto ret = db[col].replace_one(make_document(kvp("_id", id)) ,
                bsoncxx::from_json(doc));
        return ret.value().modified_count() > 0;
    }catch(std::exception& e){
        LOG(error) << e.what();
        return false;
    }
    return false;
}
const std::string Mongo::find_mony( const std::string col, const std::string doc)
{
    LOG(trace) << "col:" << col << " doc:" << doc;
    try{
        auto result = db[col].find(bsoncxx::from_json(doc));
        std::string result_str;
        for(const auto& e : result){
            result_str += bsoncxx::to_json(e) + ","; 
        }
        if(result_str.size() > 0) result_str.pop_back();
        return "[" + result_str + "]";
    }catch(std::exception& e){
        LOG(error) << e.what();
    }
    return "[]";
}
const std::string Mongo:: find_one( const std::string col, const std::string doc)
{
    LOG(trace) << "col:" << col << " doc:" << doc;
    try{
        auto result = db[col].find_one(bsoncxx::from_json(doc));
        if (result && !(result->view().empty())) {
            bsoncxx::document::view  v(result->view());
            return bsoncxx::to_json(v); 
        }
    }catch(std::exception& e){
        LOG(error) << e.what();
    }
    return "{}";
}
const std::string Mongo:: find_id(const std::string col, int64_t id)
{
    LOG(trace) << "col:" << col << " id:" << id;
    try{
        auto result = db[col].find_one(make_document(kvp("_id", id)));
        if (result && !(result->view().empty())) {
            bsoncxx::document::view  v(result->view());
            return bsoncxx::to_json(v); 
        }
    }catch(std::exception& e){
        LOG(error)  << e.what();
    }
    return "{}";
}
int64_t Mongo::get_uniq_id()
{
    std::string col = "uniq_counter"; 
    int64_t id = 1;
    try{
        mongocxx::options::find_one_and_update options {};
        options.return_document(mongocxx::options::return_document::k_after);
        auto result = db[col].find_one_and_update(
                make_document(kvp("_id", id)),
                make_document(kvp("$inc", make_document(kvp("count", 1)))),
                options);
        if (result && !(result->view().empty())) {
            bsoncxx::document::view  v(result->view());
            auto type =  v["count"].type();
            int64_t newId = 0;
            if(type == bsoncxx::type::k_double)      newId = v["count"].get_double();
            else if(type == bsoncxx::type::k_int64)  newId = v["count"].get_int64();
            else if(type == bsoncxx::type::k_int32)  newId = v["count"].get_int32();
            else{
                LOG(error) << "Invalid type of 'count': " << bsoncxx::to_string(type);
            }
            return newId; 
        }
        return 1; 
    }catch(std::exception& e){
        LOG(error) << e.what();
        return 1;
    }
}
const std::string Mongo:: find_range(const std::string col, long begin, long end)
{
    LOG(trace) << "col:" << col << " begin:" << begin << " end:" << end;
    try{
        mongocxx::options::find options {};
        options.skip(begin);
        options.limit(end - begin + 1);
        bsoncxx::document::view  v;
        long total = db[col].count_documents(v);
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
        LOG(error) << e.what();
        return "{ \"total\": 0, \"content\":[] }";
    }
}
const std::string Mongo:: find_filter_range(const std::string col, 
        const std::string filter,
        long begin, long end)
{
    LOG(trace) << " begin:" << begin << " end:" << end
        << " col:" << col <<" filter:" << filter;
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
        long total = db[col].count_documents(bsoncxx::from_json(filter));
        return "{ \"total\": " + std::to_string(total) + 
            ", \"content\":[" + result_str + "] }";
    }catch(std::exception& e){
        LOG(error) << e.what();
        return "{ \"total\": 0, \"content\":[] }";
    }
}


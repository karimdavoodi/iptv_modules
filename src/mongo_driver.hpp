#pragma onc;
#include <iostream>
#include <boost/log/trivial.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/logger.hpp>
#include <mongocxx/options/client.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/stdx/make_unique.hpp>
#define  QUERY_SIZE 10
namespace Mongo {
    void fill_defauls();
    bool exists(const std::string col, const std::string filter);
    bool exists_id(const std::string col, int id);
    bool insert(const std::string col, const std::string filter);
    bool insert_id(const std::string col, int id, 
            const std::string doc);
    bool remove_mony(const std::string col, const std::string filter);
    bool remove_id(const std::string col, int id);
    bool replace_id(const std::string col, int id, 
            const std::string doc);
    bool insert_or_replace_id(const std::string col, int id, 
            const std::string doc);
    bool replace(const std::string col, const std::string filter, 
            const std::string doc);
    int get_uniq_id();
    const std::string find_one(const std::string col, const std::string filter);
    const std::string find_mony(const std::string col, const std::string filter);
    const std::string find_id(const std::string col, int id);
    const std::string find_range(const std::string col, 
            int begin = 0, int end = QUERY_SIZE);
    const std::string find_filter_range(const std::string col, 
            const std::string filter,
            int begin = 0, int end = QUERY_SIZE);
    void info();
};

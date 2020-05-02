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


class Mongo {
    public:
        static void fill_defauls();
        static bool exists(std::string col, std::string filter);
        static bool exists_id(std::string col, int id);
        static bool insert(std::string col, std::string filter);
        static bool remove(std::string col, std::string filter);
        static bool remove_by_id(std::string col, int id);
        static bool replace_by_id(std::string col, int id, 
                                    std::string doc);
        static bool replace(std::string col, std::string filter, 
                                    std::string doc);
        static std::string find(std::string col, std::string filter);
        static std::string find_id(std::string col, int id);
        static std::string find_id_range(std::string col, 
                                    int begin, int end);
        static std::string find_time_id_range(std::string col,long stime, long etime, 
                                    int begin, int end);
        static void info();
};

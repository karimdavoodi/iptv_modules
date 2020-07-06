#pragma onc;
#include <iostream>
#include <mutex>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#define QUERY_SIZE  10
#define DB_NAME     "iptv"
#define SERVER      "mongodb://127.0.0.1:27017"
class Mongo {
    private:
        mongocxx::client client{mongocxx::uri{SERVER}};
        std::mutex db_mutex;
        mongocxx::database db;
    public:
        Mongo():client{mongocxx::uri{SERVER}},
                db_mutex{}
                { db = client[DB_NAME]; }
        void fill_defauls();
        bool exists(const std::string col, const std::string filter);
        bool exists_id(const std::string col, long id);
        bool insert(const std::string col, const std::string filter);
        bool insert_id(const std::string col, long id, 
                const std::string doc);
        bool remove_mony(const std::string col, const std::string filter);
        bool remove_id(const std::string col, long id);
        bool replace_id(const std::string col, long id, 
                const std::string doc);
        bool insert_or_replace_id(const std::string col, long id, 
                const std::string doc);
        bool replace(const std::string col, const std::string filter, 
                const std::string doc);
        long get_uniq_id();
        const std::string find_one(const std::string col, const std::string filter);
        const std::string find_mony(const std::string col, const std::string filter);
        const std::string find_id(const std::string col, long id);
        const std::string find_range(const std::string col, 
                long begin = 0, long end = QUERY_SIZE);
        const std::string find_filter_range(const std::string col, 
                const std::string filter,
                long begin = 0, long end = QUERY_SIZE);
        void info();
};

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
#pragma once
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
        long count(const std::string col, const std::string filter);
        bool exists(const std::string col, const std::string filter);
        bool exists_id(const std::string col, int64_t id);
        bool insert(const std::string col, const std::string filter);
        bool insert_id(const std::string col, int64_t id, 
                const std::string doc);
        bool remove_mony(const std::string col, const std::string filter);
        bool remove_id(const std::string col, int64_t id);
        bool replace_id(const std::string col, int64_t id, 
                const std::string doc);
        bool insert_or_replace_id(const std::string col, int64_t id, 
                const std::string doc);
        bool replace(const std::string col, const std::string filter, 
                const std::string doc);
        bool insert_or_replace_filter(const std::string col, const std::string filter,  
                                            const std::string doc);
        int64_t get_uniq_id();
        const std::string find_one(const std::string col, const std::string filter);
        const std::string find_mony(const std::string col, const std::string filter);
        const std::string find_id(const std::string col, int64_t id);
        const std::string find_range(const std::string col, 
                long begin = 0, long end = QUERY_SIZE);
        const std::string find_filter_range(const std::string col, 
                const std::string filter,
                long begin = 0, long end = QUERY_SIZE);
        void info();
};

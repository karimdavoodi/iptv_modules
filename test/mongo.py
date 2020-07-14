#!/usr/bin/python3
# -*- coding: utf-8 -*-
from pymongo import MongoClient
import time
import os
import json

class Mongo(object):
    def __init__(self):
        self.client = MongoClient('localhost', 27017)
        self.db = self.client['iptv']
        self.init_db()

    def init_db(self):
        print("Init 'iptv' DB")
        with open("def.json") as f:
            defaults = json.load(f)
            for key, val in defaults.items():
                if type(val) == dict:
                    self.insert_or_replace_id(key, val['_id'] ,val)
                if type(val) == list:
                    for doc in val:
                        self.insert_or_replace_id(key, doc['_id'] ,doc)


    def info(self):
        return self.db.list_collection_names()

    def count(self, collection, doc={}):
        return self.db[collection].count_documents(doc)

    def clear(self, collection, doc = {}):
        self.db[collection].delete_many(doc)

    def insert(self, collection, doc):
        if self.db[collection].count_documents(doc) == 0:
            self.db[collection].insert_one(doc)
        #else:
        #    self.error("Doc is exists. not insert to " + collection)

    def insert_id(self, collection, _id, doc):
        if self.db[collection].count_documents({'_id':_id}) == 0:
            self.db[collection].insert_one(doc)
        #else:
        #    self.error("Doc is exists. not insert to " + collection)

    def insert_or_replace_id(self, collection, _id, doc):
        if self.db[collection].count_documents({'_id':_id}) == 1:
            self.db[collection].replace_one({'_id':_id}, doc)
        else:
            self.db[collection].insert_one(doc)

    def find(self, collection, query = {}):
        cursor = self.db[collection].find(query)
        if cursor.count() < 1: return "EMPTY"
        c_list = []
        for c in cursor: c_list.append(c)
        return c_list

    def find_one(self, collection, query):
        return self.db[collection].find_one(query)

    def find_id(self, collection, _id):
        return self.db[collection].find_one({'_id':_id})

    def error(self, msg, priority = 1):
        _id = time.time()
        self.db['report_error'].insert_one({
            '_id': _id,
            'time': int(_id),
            'message': msg,
            'process': "sms_main",
            'level': priority
            })

if __name__ == "__main__":
    print("Init DB start")
    mdb = Mongo()
    print("Init DB end")

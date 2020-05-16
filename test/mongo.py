#!/usr/bin/python
# -*- coding: utf-8 -*-
from pymongo import MongoClient
import time
import os
import json

class Mongo(object):
    def __init__(self):
        self.client = MongoClient('localhost', 27017)
        if 'iptv' not in self.client.list_database_names():
            self.db = self.client['iptv']

        self.db = self.client['iptv']

    def info(self):
        return self.db.list_collection_names()

    def count(self, collection, doc={}):
        return self.db[collection].count_documents(doc)

    def clear(self, collection, doc = {}):
        self.db[collection].delete_many(doc)

    def insert(self, collection, doc):
        if self.db[collection].find(doc).count() == 0:
            self.db[collection].insert(doc)
        else:
            self.error("Doc is exists. not insert to " + collection)

    def insert_id(self, collection, doc, _id):
        if self.db[collection].find({'_id':_id}).count() == 1:
            self.db[collection].replace_one({'_id':_id}, doc)
        else:
            self.db[collection].insert(doc)

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
        now = time.time()
        self.db['status_errors'].insert({
            '_id': now,
            'time': now,
            'error': msg,
            'priority': priority
            })

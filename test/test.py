#!/usr/bin/python
# -*- coding: utf-8 -*-
import os
import time
import subprocess
import imp
mongo = imp.load_source('mongo', '/s/uicontrol/mongo.py')


def make_channel_config(chan_type, number):
    print(("Insert channel type:" + chan_type))

    mdb = mongo.Mongo()
    live_inputs_types = mdb.find_one("live_inputs_types",{"name":chan_type})
    if live_inputs_types == None:
        print(("Invalid channel type:"+chan_type))
        return
    channel_type = live_inputs_types["_id"]
    for ii in range(number):
        i = ii + 1000671
        if chan_type == "dvb":
            name = "IRIB TV"+str(i)
            mdb.insert_or_replace_id("live_inputs_dvb", i,{  
                "_id": i,
                "active": True,
                "name": name, 
                "dvb_id": 1, 
                "is_dvbt": True, 
                "sid": 100+i, 
                "aid": 1010+i, 
                "vid": 100+i,
                "freq": 602000, 
                "pol": 1, 
                "scramble": False, 
                "symb": 1
                })
            mdb.insert_or_replace_id("live_tuners_input", 1,{  
                "_id": 1,
                "name": "t1", 
                "active": True,
                "is_dvbt": True, 
                "freq": 650000,        # from 10000 .. 1000000
                "errrate": "",  # from 'AUTO', '1/2', '1/3', '3/4', '5/6', '7/8'
                "pol": "",      # from 'H', 'V'
                "symrate": 0,     # from 1 .. 50000
                "switch": 0       # from 1 .. 4
                })
        if chan_type == "archive":
            name = "hdd "+str(i)
            mdb.insert_or_replace_id("live_inputs_archive", i,{  
                "_id": i,
                "name": name,
                "active": True,
                "isTV": True, 
                "manualSchedule": False,
                "contents": [ 
                        {
                        "content": 3001+ii,
                        "weekday": 1,
                        "time": 1,
                        }
                    ],
                })
        if chan_type == "network":
            name = "net"+str(i)
            mdb.insert_or_replace_id("live_inputs_network", i,{  
                "_id": i,
                "active": True,
                "name": name,
                #"url": "http://192.168.56.12:8001/34/hdd11.ts", 
                "url": "rtsp://192.168.56.12:554/iptv/239.1.1.2/3200", 
                "static": True
                })
        if chan_type == "web":
            name = "web"+str(i)
            mdb.insert_or_replace_id("live_inputs_web", i,{  
                "_id": i,
                "active": True,
                "name": name,
                "url": "http://www.cnn.com", 
                "static": True
                })
        if chan_type == "virtual_net":
            name = "virtual_net"+str(i)
            mdb.insert_or_replace_id("live_inputs_virtual_net", i,{  
                "_id": i,
                "active": True,
                "name": name, 
                "url": "http://virtual_net/url", 
                "record": True,
                "permission": 1
                })
        if chan_type == "virtual_dvb":
            name = "virtual_dvb"+str(i)
            mdb.insert_or_replace_id("live_inputs_virtual_dvb", i,{  
                "_id": i,
                "active": True,
                "name": name, 
                "freq": (602+i)*1000, 
                "sid": 102+i, 
                "record": True,
                "permission": 1
                })
        if chan_type == "transcode":
            name = "transcode"+str(i)
            mdb.insert_or_replace_id("live_inputs_transcode", i,{  
                "_id": i,
                "active": True,
                "name": name, 
                "input": i, 
                "inputType": 2,
                "profile": 100072
                })
        if chan_type == "unscramble":
            name = "unscramble"+str(i)
            mdb.insert_or_replace_id("live_inputs_unscramble", i,{  
                "_id": i,
                "active": True,
                "input": i, 
                "inputType": 2,
                "bssKey": "",
                "cccam": 1
                })
        if chan_type == "scramble":
            name = "scramble"+str(i)
            mdb.insert_or_replace_id("live_inputs_scramble", i,{  
                "_id": i,
                "active": True,
                "input": i, 
                "inputType": 2,
                "crypto": "AES",
                "key": "123" 
                })
        if chan_type == "mixed":
            name = "mixed"+str(i)
            mdb.insert_or_replace_id("live_inputs_mixed", i,{  
                "_id": i,
                "active": True,
                "input1": i, 
                "input2": i, 
                "inputType1": 2,    # from live/inputs/types 
                "inputType2": 3,    # from live/inputs/types 
                "input2mix": 'cover',  # from 'multiple','cover'
                "input2x": 1, 
                "input2y": 1, 
                "input2width": 100, 
                "input2height": 100 
                })
        silver_id =  channel_type*100+i
        mdb.insert_or_replace_id("live_output_silver", silver_id, {
            "_id": silver_id,
            "active": True,
            "input": i,
            "inputType": channel_type,
            "name": name, 
            "permission": 1,
            "category":[channel_type],
            "freq": 407000, 
            "sid": silver_id, 
            "recordTime": 1,
            "udp": True,
            "http": True,
            "rtsp": True,
            "hls": True
            })


def init_db():
    mdb = mongo.Mongo()
    mdb.insert_or_replace_id("system_network", 1, {
        "_id": 1,
        "interfaces":[
                {
                "_id": 1,
                "name": "wlo1",
                "description": "",
                "ip": "192.168.43.61",  
                "mask": "255.255.255.0",
                },
            ],
        "dns": "8.8.8.8",
        "gateway": "192.168.43.1",
        "mainInterface": 1,
        "multicastBase": 239,
        "multicastInterface": 1,
        "addressForNAT": "",
        "staticRoute": [ ],
        "firewallRule": [ ]
        })

def add_video(path):
    mdb = mongo.Mongo()
    root_path = "/opt/sms/www/iptv/media/Video/"
    poster_path = "/opt/sms/www/iptv/media/Poster/"
    os.system("mkdir -p "+root_path)
    os.system("mkdir -p "+poster_path)
    _id = 3000
    t = int(time.time())
    for root, dirs, files in os.walk(path):
        for name in files:
            file_path = os.path.join(root, name)
            fmt = 0
            fmt_name = ""
            if ".ts" in name: 
                fmt = 1
                fmt_name = ".ts"
            elif ".mp4" in name: 
                fmt =  2
                fmt_name = ".mp4"
            elif ".mkv" in name: 
                fmt =  3
                fmt_name = ".mkv"
            if fmt == 0: continue
            _id += 1
            os.system("cp " + file_path + " " + root_path + str(_id) + fmt_name )        
            chname = name[:-4] 
            print(("add " + file_path))
            mdb.insert_or_replace_id("storage_contents_info", _id, {
                    "_id": _id,
                    "type":3,
                    "format": fmt,
                    "category":[ _id % 3 + 1 ],
                    "name": chname,
                    "permission": [],
                    "price": 0,
                    "platform":[],
                    "date": t,
                    "languages" : [], 
                    "description":{
                            "en": {
                                "name": chname,
                                "description": ""
                                },
                            "fa": {
                                "name": chname,
                                "description": ""
                                },
                            "ar": {
                                "name": chname,
                                "description": ""
                            }
                        }
                    })
def add_picture(path):
    mdb = mongo.Mongo()
    root_path = "/opt/sms/www/iptv/media/Picture/"
    poster_path = "/opt/sms/www/iptv/media/Poster/"
    os.system("mkdir -p "+root_path)
    os.system("mkdir -p "+poster_path)
    _id = 2000
    t = int(time.time())
    for root, dirs, files in os.walk(path):
        for name in files:
            file_path = os.path.join(root, name)
            fmt = 0
            fmt_name = ""
            if ".png" in name: 
                fmt = 8
                fmt_name = ".png"
            elif ".gif" in name: 
                fmt =  9
                fmt_name = ".gif"
            elif ".jpg" in name: 
                fmt =  10
                fmt_name = ".jpg"
            if fmt == 0: continue
            _id += 1
            os.system("cp " + file_path + " " + root_path + str(_id) + fmt_name )        
            os.system("convert " + file_path + " resize 240x240 " 
                    + poster_path + str(_id) + ".jpg" )        
            chname = name[:-4] 
            mdb.insert_or_replace_id("storage_contents_info", _id, {
                    "_id": _id,
                    "type":5,
                    "format": fmt,
                    "category":[ _id % 3 + 1 ],
                    "name": chname,
                    "permission": [],
                    "price": 0,
                    "platform":[],
                    "date": t,
                    "languages" : [], 
                    "description":{
                            "en": {
                                "name": chname,
                                "description": ""
                                },
                            "fa": {
                                "name": chname,
                                "description": ""
                                },
                            "ar": {
                                "name": chname,
                                "description": ""
                            }
                        }
                    })
def add_menu():
    mdb = mongo.Mongo()
    menu = []
    for i in range(1,9):
        type_ = mdb.find_id("launcher_components_types",i)
        type_name = type_["name"]
        comp = []
        for j in range(1,4):
            _id = 100 + i*10 + j 
            comp.append(_id)
            mdb.insert_or_replace_id("launcher_components_info", _id, {
                    "_id": _id,
                    "active": True,
                    "type": i,
                    "name":{
                        "en": type_name + " en " + str(j),
                        "fa": type_name + " fa " + str(j),
                        "ar": type_name + " ar " + str(j)
                    }, 
                    "logo": 57+j,
                    "url":"",
                    "platform":[],
                    "category":[j]
                    })

        _id = 100 + i 
        menu.append(_id)
        mdb.insert_or_replace_id("launcher_menu", _id, {
            "_id": _id,
            "active": True,
            "name":{
                "en": type_name + " en",
                "fa": type_name + " fa",
                "ar": type_name + " ar"
            }, 
            "logo": 54+i,
            "permission": 1,
            "components": comp 
                })
    menu.append(1)
    menu.append(2)
    menu.append(3)
    mdb.insert_or_replace_id("launcher_setting", 10, {
            "_id": 10,
            "active": True,
            "name": "Line",
            "menu": menu,
            "icons":{
                "background": 1,
                "company":19,
                "net_ok":10,
                "net_nok":22,
                "lock":3,
                "unlock":16,
                "logo": {
                    "en": 15,
                    "fa": 15,
                    "ar": 15
                }
            }
            ,"font":{
                "en": "EN01.ttf",
                "fa": "FA_Entezar.ttf",
                "ar": "FA_Entezar.ttf"
            },
            "welcome":{
                "en": "Welcome",
                "fa": "خوش آمدید",
                "ar": "خوش آمدید"
            },
            "user":{
                "en": "Guest",
                "fa": "میهمان",
                "ar": "میهمان"
            },
            "welcomeClip": True,
            "welcomeClipMedia": {
                "en":1,
                "fa":1,
                "ar":1
            },
            "unitName": "Unit",
            "clientHotspot": "",
            "defaultChannel": 1 
            })
"""
add_video("/home/karim/Music/Video_Music")
add_menu()
add_picture("/home/karim/Pictures/mypic/990220")
init_db()
"""
for ch_type in ["dvb", "archive", "network", "web", "virtual_dvb", "virtual_net",
        "transcode", "scramble", "unscramble", "mixed"]:
    make_channel_config(ch_type, 10)


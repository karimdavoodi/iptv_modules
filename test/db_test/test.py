#!/usr/bin/python
# -*- coding: utf-8 -*-
import os
import time
import subprocess
import imp
mongo = imp.load_source('mongo', '/s/uicontrol/mongo.py')


def make_input_channels_profile():
    mdb = mongo.Mongo()
    mdb.insert_or_replace_id("live_profiles_scramble", 1,{  \
                "_id": 1,
                "active": True,
                "name": "sc1",
                "offline":{
                    "algorithm": "BISS",   # from 'BISS', 'AES128', 'AES256' 
                    "key": "123456123456" 
                },
                "online":{
                    "protocol": "",  # from "cccam","camd35","cs378x","gbox","newcamd"
                    "server": "", 
                    "user": "", 
                    "pass": "" 
                }
            })
    mdb.insert_or_replace_id("live_profiles_transcode", 1,{  \
                "_id": 1,
                "active": True,
                "name": "trans_sd1", 
                "preset": "ultrafast", 
                "videoCodec": "h264",   # from  '','h265','h264','mpeg2' 
                "videoSize": "SD",    # from  '','4K', 'FHD', 'HD', 'SD', 'CD' 
                "videoRate": 3000000,       # from 1 .. 100000000  (in bps) 
                "videoFps": 25,        # from 1 .. 60
                "videoProfile": "", # from  '','Baseline', 'Main', 'High'
                "audioCodec": "mp3",   # from  '','aac','mp3','mp2' 
                "audioRate": 128000,       # from 1 to 1000000 (in bps)
                "extra": "" 
        })
    mdb.insert_or_replace_id("live_profiles_mix", 1,{  
                "_id": 1,
                "active": True,
                "name": "mix1", 
                "input1": {
                    "useVideo": True,
                    "useAudio": True,
                    "audioNumber": 1
                    },
                "input2": {
                    "useVideo": True,
                    "useAudio": True,
                    "audioNumber": 1,
                    "whiteTransparent": True,
                    "posX": 1,
                    "posY": 1,
                    "width": 1000,
                    "height": 400
                    },
                "output":{          # use if we mix two video
                    "width": 1280,
                    "height": 720,
                    "bitrate": 4000000
                    }
        })

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
            mdb.insert_or_replace_id("live_tuners_info", 1,{  
                       "_id": 1,
                       "active": True,
                       "name": "dvb1", 
                       "description": "",
                       "dvbt": True,
                       "systemId": 0,          # from tuners/system
                       "frequencyId": 1,       # from satellites/frequencies
                       "diSEqC": 1,            # from 1 .. 4
                       "virtual": False
                })
            name = "IRIB TV"+str(i)
            mdb.insert_or_replace_id("live_inputs_dvb", i,{  
                "_id": i,
                "active": True,
                "name": name, 
                "logo": 1, 
                "dvbId": 1,           # from tuners/info
                "channelId": 1,       # from satellite/channels
                "tv": True 
                })
        if chan_type == "archive":
            name = "hdd "+str(i)
            mdb.insert_or_replace_id("live_inputs_archive", i,{  
                "_id": i,
                "active": True,
                "name": name, 
                "logo": 1,                    # from storage/contents/info  type 'Logo'
                "description": "",
                "tv": True,
                "contents":[
                    {
                    "content": 3001, 
                    "weektime": 1,           # from system/weektime
                    "startDate": 1500000000,
                    "endDate": 1600000000
                    },
                    {
                    "content": 3002, 
                    "weektime": 1,           # from system/weektime
                    "startDate": 1500000000,
                    "endDate": 1600000000
                    }
                    ]
                })
        if chan_type == "network":
            name = "net"+str(i)
            mdb.insert_or_replace_id("live_inputs_network", i,{  
                "_id": i,
                "active": True,
                "name": name,
                "logo": 1,                # from storage/contents/info  type 'Logo'
                "channelId": "",        # from live/network/channels (optional)
                "url": "rtsp://192.168.56.12:554/iptv/239.1.1.2/3200", 
                "static": True,
                "virtual": False,
                "webPage": False,
                "tv": True
                })
        if chan_type == "transcode":
            name = "transcode"+str(i)
            mdb.insert_or_replace_id("live_inputs_transcode", i,{  
                "_id": i,
                "active": True,
                "name": name, 
                "input": i,        # from live/inputs
                "inputType": 2,    # from live/inputs/types 
                "profile": 1,      # from live/transcode_profile
                })
        if chan_type == "scramble":
            name = "scramble"+str(i)
            mdb.insert_or_replace_id("live_inputs_scramble", i,{  
                "_id": i,
                "active": True,
                "name": name, 
                "input": i,           # from live/inputs
                "inputType": 2,       # from live/inputs/types 
                "decrypt": True,     # 'True' for unscrambling and 'False' for scrambling 
                "profile": 1          # from live/scramble_profile
                })
        if chan_type == "mix":
            name = "mix"+str(i)
            mdb.insert_or_replace_id("live_inputs_mix", i,{  
                "_id": i,
                "active": True,
                "name": name, 
                "profile": 1,          # from live/mix_profile
                "input1": i,           # from live/inputs
                "inputType1": 2,       # from live/inputs/types 
                "input2": i,           # from live/inputs
                "inputType2": 2,       # from live/inputs/types 
                })
        ch_id =  channel_type*100+i
        mdb.insert_or_replace_id("live_output_network", ch_id, {
               "_id": ch_id,
               "active": True,
               "name": chan_type + str(i), 
               "input": i,    
               "inputType": channel_type,   # from live/inputs/types 
               "category": [i % 3],
               "udp": True,
               "http": True,
               "rtsp": True,
               "hls": True,
            })
        ch_id =  channel_type*100+i
        mdb.insert_or_replace_id("live_output_dvb", ch_id, {
                "_id": ch_id,
                "active": True,
                "input": i,       # from live/inputs
                "inputType": channel_type,   # from live/inputs/types 
                "name": chan_type + str(i), 
                "category": [i % 2],  # from storage/contents/categories
                "dvbId": 1, 
                "serviceId": 101, 
            })
        ch_id =  channel_type*100+i
        mdb.insert_or_replace_id("live_output_archive", ch_id, {
               "_id": ch_id,
               "active": True,
               "name": chan_type + str(i), 
               "input": i,    
               "inputType": channel_type,   # from live/inputs/types 
               "category": [i % 2],  # from storage/contents/categories
               "timeShift": 1,
               "programName": "",
               "virtual": False,
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

def add_audio(path):
    mdb = mongo.Mongo()
    root_path = "/opt/sms/www/iptv/media/Audio/"
    poster_path = "/opt/sms/www/iptv/media/Poster/"
    os.system("mkdir -p "+root_path)
    os.system("mkdir -p "+poster_path)
    _id = 3300
    t = int(time.time())
    for root, dirs, files in os.walk(path):
        for name in files:
            file_path = os.path.join(root, name)
            fmt = 0
            fmt_name = ""
            if ".mp3" in name: 
                fmt = 4
                fmt_name = "mp3"

            if fmt == 0: continue
            _id += 1
            os.system("cp " + file_path + " " + root_path + str(_id) + "." + fmt_name )        
            chname = name[:-4] + " MP3" 
            print(("add " + file_path))
            mdb.insert_or_replace_id("storage_contents_info", _id, {
                    "_id": _id,
                    "type":4,
                    "format": fmt_name,
                    "category":[  1 ],
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
                fmt_name = "ts"
            elif ".mp4" in name: 
                fmt =  2
                fmt_name = "mp4"
            elif ".mkv" in name: 
                fmt =  3
                fmt_name = "mkv"
            if fmt == 0: continue
            _id += 1
            os.system("cp " + file_path + " " + root_path + str(_id) + "." + fmt_name )        
            chname = name[:-4] 
            print(("add " + file_path))
            mdb.insert_or_replace_id("storage_contents_info", _id, {
                    "_id": _id,
                    "type":3,
                    "format": fmt_name,
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
                fmt_name = "png"
            elif ".gif" in name: 
                fmt =  9
                fmt_name = "gif"
            elif ".jpg" in name: 
                fmt =  10
                fmt_name = "jpg"
            if fmt == 0: continue
            _id += 1
            os.system("cp " + file_path + " " + root_path + str(_id) + "." + fmt_name )        
            os.system("convert " + file_path + " resize 240x240 " 
                    + poster_path + str(_id) + ".jpg" )        
            chname = name[:-4] 
            mdb.insert_or_replace_id("storage_contents_info", _id, {
                    "_id": _id,
                    "type":5,
                    "format": fmt_name,
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
add_audio("/home/karim/Music/mp3")
add_video("/home/karim/Music/Video_Music")
add_menu()
add_picture("/home/karim/Pictures/mypic/990220")
init_db()
"""
make_input_channels_profile()
for ch_type in ["dvb", "archive", "network", "transcode", "scramble", "mix"]:
    make_channel_config(ch_type, 2)


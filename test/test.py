import os
import time
import commands
import mongo

def test_transcode():
    print("Test iptv_in_dvb\ninit db")
    mdb = mongo.Mongo()
    mdb.insert("live_inputs_transcode", {
        "_id": 2,
        "active": True,
        "name": "hdd2_ssd", 
        "input": 1, 
        "inputType": 2,
        "profile": 1,
        })
    mdb.insert("live_inputs_transcode", {
        "_id": 3,
        "active": True,
        "name": "hdd3_ssd", 
        "input": 1, 
        "inputType": 2,
        "profile": 2,
        })
    mdb.insert("live_output_silver", {
        "_id": 21,
        "active": True,
        "inputId": 2,
        "inputType": 7,
        "name": "net1", 
        "permission": 1,
        "category":[],
        "freq": 0, 
        "sid": 0, 
        "recordTime": 0,
        "udp": True,
        "http": True,
        "rtsp": True,
        "hls": True
        })
    mdb.insert("live_output_silver", {
        "_id": 22,
        "active": True,
        "inputId": 3,
        "inputType": 7,
        "name": "net1", 
        "permission": 1,
        "category":[],
        "freq": 0, 
        "sid": 0, 
        "recordTime": 0,
        "udp": True,
        "http": True,
        "rtsp": True,
        "hls": True
        })

def test_dvb():
    print("Test iptv_in_dvb\ninit db")
    mdb = mongo.Mongo()
    mdb.insert("live_inputs_dvb", {
        "_id": 2,
        "active": True,
        "name": "IRIB TV1", 
        "dvb_id": 1, 
        "is_dvbt": True, 
        "sid": 101, 
        "aid": 1011, 
        "vid": 101,
        "freq": 602000, 
        "pol": 1, 
        "scramble": False, 
        "symb": 1
        })
    mdb.insert("live_inputs_dvb", {
        "_id": 3,
        "active": True,
        "name": "IRIB TV2", 
        "dvb_id": 1, 
        "is_dvbt": True, 
        "sid": 102, 
        "aid": 1021, 
        "vid": 102,
        "freq": 602000, 
        "pol": 1, 
        "scramble": False, 
        "symb": 1
        })
    mdb.insert("live_inputs_dvb", {
        "_id": 4,
        "active": True,
        "name": "IRIB TV4", 
        "dvb_id": 2, 
        "is_dvbt": True, 
        "sid": 102, 
        "aid": 1021, 
        "vid": 102,
        "freq": 602000, 
        "pol": 1, 
        "scramble": False, 
        "symb": 1
        })
    mdb.insert("live_output_silver", {
        "_id": 10,
        "active": True,
        "inputId": 2,
        "inputType": 1,
        "name": "net1", 
        "permission": 1,
        "category":[],
        "freq": 0, 
        "sid": 0, 
        "recordTime": 0,
        "udp": True,
        "http": True,
        "rtsp": True,
        "hls": True
        })
    mdb.insert("live_output_silver", {
        "_id": 11,
        "active": True,
        "inputId": 3,
        "inputType": 1,
        "name": "net1", 
        "permission": 1,
        "category":[],
        "freq": 0, 
        "sid": 0, 
        "recordTime": 0,
        "udp": True,
        "http": True,
        "rtsp": True,
        "hls": True
        })
    mdb.insert("live_output_silver", {
        "_id": 12,
        "active": True,
        "inputId": 4,
        "inputType": 1,
        "name": "net1", 
        "permission": 1,
        "category":[],
        "freq": 0, 
        "sid": 0, 
        "recordTime": 0,
        "udp": True,
        "http": True,
        "rtsp": True,
        "hls": True
        })

def test_network():
    print("Test iptv_in_network\ninit db")
    mdb = mongo.Mongo()
    mdb.insert("live_inputs_network", {
        "_id": 4,
        "active": True,
        "name": "net1_rtsp",
        "url": "rtsp://192.168.56.12:554/iptv/239.1.1.2/3200", 
        "static": True
        })
    mdb.insert("live_inputs_network", {
        "_id": 5,
        "active": True,
        "name": "net2_http",
        "url": "http://192.168.56.12:8001/34/hdd11.ts", 
        "static": True
        })
    mdb.insert("live_output_silver", {
        "_id": 34,
        "active": True,
        "inputId": 4,
        "inputType": 3,
        "name": "net1_rtsp", 
        "permission": 1,
        "category":[],
        "freq": 0, 
        "sid": 0, 
        "recordTime": 0,
        "udp": True,
        "http": True,
        "rtsp": True,
        "hls": True
        })
    mdb.insert("live_output_silver", {
        "_id": 35,
        "active": True,
        "inputId": 5,
        "inputType": 3,
        "name": "net2_http", 
        "permission": 1,
        "category":[],
        "freq": 0, 
        "sid": 0, 
        "recordTime": 0,
        "udp": True,
        "http": True,
        "rtsp": True,
        "hls": True
        })
#    print("run app, and wait 5 sec")
#    before = commands.getoutput("netstat -s | grep OutMcastPkts").split()[1]
#    os.system("../iptv_in_network &")
#    time.sleep(5)
#    print("check out")
#    os.system("killall iptv_in_network")
#    after = commands.getoutput("netstat -s | grep OutMcastPkts").split()[1]
#    print("Multicast Packets: Before %s After %s"%(before, after))
#    if int(after) - int(before) < 100 :
#        print("Error in iptv_in_archive")
#    else:
#        print("OOOOOOKKKKKKK iptv_in_network")

def test_archive():
    print("Test iptv_in_archive\ninit db")
    mdb = mongo.Mongo()
    mdb.insert("system_network", {
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
        "multicastBase": "239.1.0.0",
        "multicastInterface": 1,
        "addressForNAT": "",
        "staticRoute": [ ],
        "firewallRule": [ ]
        })
    mdb.insert("live_inputs_archive", {
        "_id": 1,
        "name": "hdd1",
        "active": True,
        "isTV": True, 
        "manualSchedule": False,
        "contents": [ 
                {
                "content": 1001,
                "weekday": 1,
                "time": 1,
                }
            ],
        })
    mdb.insert("live_output_silver", {
        "_id": 1,
        "active": True,
        "inputId": 1,
        "inputType": 2,
        "name": "hdd1", 
        "permission": 1,
        "category":[],
        "freq": 0, 
        "sid": 0, 
        "recordTime": 0,
        "udp": True,
        "http": True,
        "rtsp": True,
        "hls": True
        })
    print("run app, and wait 5 sec")
    before = os.system("killall iptv_in_archive")
    before = commands.getoutput("netstat -s | grep OutMcastPkts").split()[1]
    os.system("../iptv_in_archive &")
    time.sleep(5)
    print("check out")
    os.system("killall iptv_in_archive")
    after = commands.getoutput("netstat -s | grep OutMcastPkts").split()[1]
    print("Multicast Packets: Before %s After %s"%(before, after))
    if int(after) - int(before) < 100 :
        print("Error in iptv_in_archive")
    else:
        print("OOOOOOKKKKKKK iptv_in_archive")

    
#test_archive()
test_network()



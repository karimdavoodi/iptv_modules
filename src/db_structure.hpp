#pragma once
const char* live_inputs_archive = 
                R"(
                    {
                       "_id": 1,
                       "active": true,
                       "description": "", 
                       "name": "", 
                       "tv": true,
                       "contents":[
                            {
                            "content": 1  
                            }
                       ]
                    }
                )";
const char* live_inputs_dvb = 
                R"(
                    {
                       "_id": 1,
                       "active": true,
                       "logo": 1, 
                       "dvbId": 1, 
                       "channelId": 1,
                       "tv": true
                    }
                )";
const char* live_inputs_network = 
                R"(
                    {
                       "_id": 1,
                       "active": true,
                       "logo": 1, 
                       "tv": true,
                       "name": "",             
                       "type": "",      
                       "description": "", 
                       "accountId": "", 
                       "channelId": "",
                       "url": "", 
                       "static": true,
                       "virtual": true,
                       "webPage": true
                    }
                )";
const char* live_inputs_transcode = 
                R"(
                    {
                       "_id": 1,
                       "active": true,
                       "logo": 1, 
                       "tv": true,
                       "name": "", 
                       "description": "", 
                       "input": 1,     
                       "inputType": 1, 
                       "profile": 1  
                    }
                )";
const char* live_inputs_mix = 
                R"(
                    {
                       "_id": 1,
                       "active": true,
                       "logo": 1, 
                       "tv": true,
                       "name": "", 
                       "description": "", 
                       "profile": 1,       
                       "input1": 1,       
                       "inputType1": 1,  
                       "input2": 1,     
                       "inputType2": 1
                    }
                )";
const char* live_inputs_scramble = 
                R"(
                    {
                       "_id": 1,
                       "active": true,
                       "logo": 1, 
                       "tv": true,
                       "name": "", 
                       "description": "", 
                       "input": 1,        
                       "inputType": 1,   
                       "decrypt": true,
                       "profile": 1   
                    }
                )";
const char* live_profiles_mix = 
                R"(
                    {
                       "_id": 1,
                       "active": true,
                        "name": "", 
                        "input1": {
                            "useVideo": true,
                            "useAudio": true,
                            "audioNumber": 1
                            },
                        "input2": {
                            "useVideo": true,
                            "useAudio": true,
                            "audioNumber": 1,
                            "whiteTransparent": true,
                            "posX": 1,
                            "posY": 1,
                            "width": 1,
                            "height": 1
                            },
                        "output":{       
                            "width": 1,
                            "height": 1,
                            "bitrate": 1
                            }
                    }
                )";
const char* live_profiles_scramble = 
                R"(
                    {
                       "_id": 1,
                       "active": true,
                        "name": "",
                        "offline":{
                            "algorithm": "", 
                            "key": "" 
                        },
                        "online":{
                            "protocol": "", 
                            "server": "", 
                            "user": "", 
                            "pass": "" 
                        }
                    }
                )";
const char* live_profiles_transcode = 
                R"(
                    {
                       "_id": 1,
                       "active": true,
                        "name": "", 
                        "preset": "",      
                        "videoCodec": "", 
                        "videoSize": "", 
                        "videoRate": 1,   
                        "videoFps": 1,       
                        "videoProfile": "",
                        "audioCodec": "", 
                        "audioRate": 1,    
                        "extra": "" 
                    }
                )";
const char* live_tuners_info = 
                R"(
                    {
                       "_id": 1,
                       "active": true,
                       "description": "",
                       "dvbt": true,
                       "systemId": 1,      
                       "frequencyId": 1,    
                       "diSEqC": 1,          
                       "virtual": true
                    }
                )";
const char* live_output_dvb = 
                R"(
                    {
                       "_id": 1,
                       "active": true,
                       "description": "", 
                       "input": 1,       
                       "inputType": 1,  
                       "category": [1],
                       "dvbId": 1, 
                       "serviceId": 1 
                    }
                )";
const char* live_output_archive = 
                R"(
                    {
                       "_id": 1,
                       "active": true,
                       "description": "", 
                       "input": 1,       
                       "inputType": 1,  
                       "category": [1],
                       "timeShift": 1,
                       "programName": "",
                       "virtual": true
                    }
                )";
const char* live_output_network = 
                R"(
                    {
                       "_id": 1,
                       "active": true,
                       "description": "", 
                       "input": 1,       
                       "inputType": 1,  
                       "category": [1],
                       "udp": true,
                       "http": true,
                       "rtsp": true,
                       "hls": true
                    }
                )";

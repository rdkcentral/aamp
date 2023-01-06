#!/usr/bin/env python3
#
# python3 -m simlinear
# simlenear
import os
from pickle import GET
import subprocess
import sys
import json
import multiprocessing
from ast import List
from http.server import BaseHTTPRequestHandler, HTTPServer
from logging import exception
import os
from re import findall
import readline
import socket
import threading
import time
from flask import Flask, Request, jsonify , request
#from flask_executor import Executor
from webargs import fields
from webargs.core import ValidateArg
from webargs.flaskparser import use_args
#import asyncio
#import aiohttp
import time
import urllib.request 
from threading import Timer, current_thread
import multiprocessing
import re
import logging
import glob
import argparse

"""
Simlinear API's:
0) Harvest VOD or LIVE content from an URL
   curl -X POST -d '{url:, durationSec:10}' 'http://localhost:5000/harvest' 
1) Start simlinear server instance running at a unique port   
    curl 'http://localhost:5000/sim-start?port=8082
    
2) Check Status of All/specific simlinear server instance       
    curl 'http://localhost:5000/sim-status?port=8082’

3) Stop simlinear server instance running at a unique port   
    curl 'http://localhost:5000/sim-stop?port=8082’

4) On-the-fly ADD stream corruption rule with a unique rule_id
   curl 'http://localhost:5000/sim-config?port=8082&cmd='add'&rule_id='err404'&assettype='video-mp4'&fragments=0,500,1000,2000&errorcode=404'
            rule_id='custom id string'
            assettypes: video-mp4/audio-mp4/sub-mp4 OR  video-regx-'*video'/audio-regx-"*audio"/sub-regex-"subtitles"
            fragments=0 to 999999999 Or -1 to -1000 (- signify skill count)
            errorcode: 404 http failures
                       502, 5xx http failures
                       1000,delay before first byte returned on a given download
                       1001,induce delay before final byte(s) for a given download (we have two flavors of client side stall abort logic)


5) On-the-fly EDIT stream corruption rule with a unique id
   curl 'http://localhost:5000/sim-config?port=8082&cmd='edit'&rule_id='err404'&assettype='video-mp4'&fragments=-1000,2000,5000&errorcode=404

6) On-the-fly DELETE stream corruption rule with a unique id
   curl 'http://localhost:5000/sim-config?port=8082&cmd='del'&rule_id='err404'
  
"""
hostName = "127.0.0.1"
serverPort = 8086

CMD_PORT=5000
#rule = port=8082&cmd='add'&rule_id='err404'&assettype='video-mp4'&fragments=0,500,1000,2000&errorcode=404
list_of_rules = {}

#{'port':port,'status':'init'/'running'/'stopped','proc':"ref to webserver instance",'process':"ref to webserver process instance",'rules':list_of_rules}
list_of_webServer = {}
list_of_threads = {}
list_of_processes = {}
havest_id = 0
#havest_process 
shutdown=False
#induce 404 http failures on specific fragment(s)
#video,audio, subtitle,init fragments
#induce temporary 502, 5xx http failures on specific fragment(s)
#induce 502, 5xx http failures for specific manifest refresh(es).
#config syntax
# "error injection policy code" "param1" "param2" "param3"
#
##ERROR INJECTION POLICY CODES filetype and params
# 1 port=8081   cmd=add/rem/edit rule_id=simple      assettype="regex-mpd?/type-mp4/type-mpd"   fragments="fragment# list(negative value=skipp_counter and positive value"   param2="errorcode"  

# 2 port=8081   rule=repetitive assettype="mpd"               param1=skippcount#        param2="errorcode" 
# 3 port=8081   rule=regex       assettype="reg-mp3"           param1="keyword"          param2="error"
#Error code list

manifest_time_adjust = 0
manifest_index = 1

video_frag_index = 0
audio_frag_index = 0
subtitle_frag_index = 0
manifest_refresh_index = 0

def normalize_query_param(value):
    """
    Given a non-flattened query parameter value,
    and if the value is a list only containing 1 item,
    then the value is flattened.

    :param value: a value from a query parameter
    :return: a normalized query parameter value
    """
    return value if len(value) > 1 else value[0]


def normalize_query(params):
    """
    Converts query parameters from only containing one value for each parameter,
    to include parameters with multiple values as lists.

    :param params: a flask query parameters data structure
    :return: a dict of normalized query parameters
    """
    params_non_flat = params.to_dict(flat=False)
    return {k: normalize_query_param(v) for k, v in params_non_flat.items()}

def is_port_available(port):   
    a_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    location = (hostName, port)
    result_of_check = a_socket.connect_ex(location)
    a_socket.close()
   
    if result_of_check == 0:
        print("Port is getting used")
        return False
    else:
        print("Port is not used")
        return True
        

class MyServer(BaseHTTPRequestHandler):
    def manifest_refresh(self,base):
        global manifest_time_adjust
        global manifest_index
        tNow = time.time()
        retryindex = 0
        
        while True:
            path = base+"." + str(manifest_index)
            print("path ",path)    
            # below doesn't survive zip/unzip
		    # mtime = os.path.getmtime(path)
		    
            # extract publishTime from 2nd line of manifest
            
            try:
                file1 = open(path, 'r')
            except FileNotFoundError:
                if retryindex >5:
                    print("EOS")
                    manifest_index-=1
                    return base+"." + str(manifest_index)
                else:
                    manifest_index+=1
                    continue
            mpd = file1.readline()
            mpd = file1.readline()
            idx = mpd.find('publishTime="')
            idx += 13
            try:
                t = time.strptime(mpd[idx:idx+24],"%Y-%m-%dT%H:%M:%S.%f%z")
            except:
                t = time.strptime(mpd[idx:idx+20],"%Y-%m-%dT%H:%M:%S%z")
            mtime = time.mktime(t)
            file1.close()
            
            if manifest_time_adjust == 0:
                manifest_time_adjust = tNow - mtime
                return path
            if tNow > manifest_time_adjust+mtime:
                manifest_index+=1
            else:
                return path

    def do_GET(self):
        temp = {}
        global list_of_rules
        global video_frag_index
        path = self.path[1:]
        print("GET ",path)
        key_list = list(list_of_threads.keys())
        val_list = list(list_of_threads.values())
        port = key_list[val_list.index(current_thread())]
       # if manifest_rule_enabled:
            
        #update index values based on the file type            
        if self.path.endswith(".mpd"):
            path = self.manifest_refresh(path)

        try: 
            # Check rule engine for all rules
            error_found = False
            #for rule in list_of_rules[port]:
            # Detect the file type  
            if  list_of_rules == {}:
                error_found = False
            else:  
                if len(list_of_rules[port]) > 0 and re.search(list_of_rules[port]['assettype'], self.path):
                    #Increment the frame number 
                    print("FRAGMENT=",list_of_rules[port]['fragment-count'])
                    list_of_rules[port]['fragment-count']+=1   
                    #if matching with the frame list issue configured error
                        
                    if (-1 in list_of_rules[port]['fraglist'] or list_of_rules[port]['fragment-count'] in list_of_rules[port]['fraglist']):
                        print ("FOUND THE MATCH")
                        contents = bytearray(0)
                        if (int(list_of_rules[port]['errorcode']) == 1000):
                            #sleep
                            print("Going to sleep")
                            time.sleep(2)
                        else:
                            self.send_response(int(list_of_rules[port]['errorcode']))
                            error_found = True

            if error_found != True:
            # TODO: can we route to default do_GET here?
             with open(path,"rb") as f:
                contents = f.read()
                self.send_response(200)
            self.send_header("Access-Control-Allow-Origin", "*")
            # self.send_header("Content-type", "text/html") - needed?
            self.end_headers()
            self.wfile.write(contents)
        except FileNotFoundError:
            self.send_response(404)
            self.end_headers()

##############################################################################################################
class HLSServer():
    """
    HTTP server specialisation for serving up HLS based content
    """
    time_of_first_manifest=0
    time_started_serving=0
    # { path_to_manifest: [(ts from mf,path_to_manifest_increment),() ... ]
    manifest_list = {}

    def get_manifest_time(self, mfile):
        """
        Read time stamp from manifest, return as epoch time
        """
        #print("get_manifest_time")
        try:
            with open(mfile, "r") as f:
                for line in f:
                    #print(line)
                    #EXT-X-PROGRAM-DATE-TIME:2022-11-14T15:59:49+0000
                    #For handling of discontinuities we support a custom tag SIMLINEAR-PDT-OVERRIDE
                    m = re.match(r'#(SIMLINEAR-PDT-OVERRIDE|EXT-X-PROGRAM-DATE-TIME):([0-9T:\-]+)',line)
                    if m:
                        time_field = m.group(2)
                        #print("field ",field)
                        d = time.strptime(time_field, "%Y-%m-%dT%H:%M:%S")
                        return time.mktime(d)

                return None
        except OSError as e:
            print(f"{type(e)}: {e}")
            return None

    def hls_parse_manifest(self,file):
        """
        Read manifest from disk, modify as needed, return
        returns {path: some_path_to_maifest } OR {contents + "manifest text"}
        """
        global list_of_rules
        print("hls_parse_manifest", file, list_of_rules)

        delay = rules.match_on_path_delay(file)
        if delay:
            #await asyncio.sleep(delay)
            time.sleep(delay)

        #Not modifying file, just return path to serve
        return {"path":file}


    def return_file_index(self,path):
        """
        Returns numeric suffix from filename
        a/b/c/manifest.abc.10  returns 10
        """
        num=0
        m= re.match(r'.*\.([0-9]+)',path)
        if m:
            num = int (m.group(1))
        return num

    def hls_get_list_of_manifest_timestamps(self,path):
        """
        Get a list of all the timestamps for this manifest
        cache the list in self.manifest_list
        e.g
        path something like test2/manifest_bitrate.m3u8
        directory test2/ contains
          manifest_bitrate.m3u8.1
          manifest_bitrate.m3u8.2
     
        returns [ (timestamp1, mf.1),(timestamp2, mf.2) ..]
        sorted by lowest file suffix first
        """
        # looking for all files ../some_manifest.m3u8.[n]
        if not path in self.manifest_list:
            gp = path + ".[0-9]*"
            print("glob ",gp)
            files = glob.glob(gp)
            files_sorted = sorted(files,key=self.return_file_index)
            #print("glob:", files_sorted)

            files_with_timestamps = []
            for f in files_sorted:
                ts = self.get_manifest_time(f)
                tup = (ts,f)
                files_with_timestamps.append(tup)
            self.manifest_list.update({path: files_with_timestamps})
        #print("hls_get_list_of_manifest_timestamps ",path, ":",self.manifest_list[path])
        return self.manifest_list[path]

    def manifest_serve(self, path):
        """
        return bytes of manifest file UTF-8 encoded
        returns {path: some_path_to_maifest } OR {contents + "manifest text"}
        """
        print("manifest_serve:",path)
        if os.path.exists(path):
            #With request for non-incremental manifest then we
            #assume this is top level manifest so reset timer
            #used for serving incremental
            self.time_started_serving = 0
            return self.hls_parse_manifest(path)
        file_timestamps = self.hls_get_list_of_manifest_timestamps(path)
        if len(file_timestamps) == 0:
            # No manifests - need return 404
            print("Not found index",path)
            return {"path":path}
        elif len(file_timestamps) == 1:
            #Occurs for VOD
            first_ts, file_to_serve = file_timestamps[0]
            return self.hls_parse_manifest(file_to_serve)
        else:
            first_ts, file_to_serve = file_timestamps[0]
            if self.time_started_serving == 0:
                 # First manifest to be served, initialise timestamps
                self.time_of_first_manifest = first_ts
                self.time_started_serving = time.time()

            # look for the timestamp and hence corresponding manifest
            # that we need to serve at this time. This will be the highest
            # number timestamp that does not exceed
            # time_manifest as calculated below
            timeElapsed = time.time() - self.time_started_serving
            time_manifest = self.time_of_first_manifest + timeElapsed

            for ts,mf in file_timestamps:
                if ts > time_manifest:
                    break
                else:
                    file_to_serve = mf
        #print("serving: now={} file_ts={}".format(time.time(),file_to_serve))
        return self.hls_parse_manifest(file_to_serve)

hls_server = HLSServer()

class HLSServerHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        path = self.path[1:]
        try:
            if ".m3u8" in self.path:
                rtn = hls_server.manifest_serve(path)
            else:
                rtn = {"path":path} #Serving a segment

            if "path" in rtn:
                print("serving: {} {}".format(time.time(),rtn["path"]))
                with open(rtn["path"],"rb") as f: contents = f.read()

            self.send_response(200)

            #Added this header so that simlinear can serve to a browser player
            # such as https://www.hlsplayer.net
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(contents)

        except FileNotFoundError:
            self.send_response(404)
            self.end_headers()

class Rules():
    list_of_rules=[]

    def new_rule(self,json):
        print("new_rule",json)
        self.list_of_rules.append(json)

    def match_on_path_delay(self,file):
        for r in self.list_of_rules:
            if "filepath" in r and "delay" in r:
                m=re.match( r["filepath"], file )
                if m:
                    return int(r["delay"])
        return 0

rules = Rules()

def start_web_server(port, abr_type):
    global list_of_webServer

    if port in list_of_webServer:
        print (f"A server is allready assigned for port = {port}")
        return
    elif abr_type == "HLS":
        #Start a HLS server
        list_of_webServer[port] =  {'status':'init','proc':HTTPServer((hostName, port), HLSServerHandler),'process':None}
        print("HLS Server started http://%s:%s" % (hostName, port))
    else:
        #Start a DASH server
        list_of_webServer[port] =  {'status':'init','proc':HTTPServer((hostName, port), MyServer),'process':None}
        print("Server started http://%s:%s" % (hostName, port))	

    try:
        list_of_webServer[port]['proc'].serve_forever()
    except KeyboardInterrupt:
        pass

    list_of_webServer[port]['proc'].server_close()
    del list_of_webServer[port]
    print("Server stopped.")

def stop_web_server(port):
    global list_of_threads
    global list_of_webServer

    if port in list_of_threads:
       list_of_webServer[port]['proc'].server_close()
       list_of_threads[port].join()
       del list_of_webServer[port]
       del list_of_threads[port]

def init_routes():
    @app.route("/sim/start",methods=["GET", "POST"])
    @use_args({"port": fields.Str(required=False)}, location="query")
    def start(args):
        global list_of_threads
        data = request.json

        if(args != {}):
            port = int(args["port"])

        #data will take residence over values in params/args
        if(data != None):
            port = int(data["port"])

        abr_type = data.get("type", "DASH")

        if is_port_available(port):
            global list_of_webServer

            thread1 = threading.Thread(target=lambda: start_web_server(port,abr_type),daemon=True)
            thread1.isDaemon = True
            list_of_threads[port]=thread1
            list_of_threads[port].start()
            return jsonify({'result':'success'}),200
        else:
            return jsonify({'result':'fail- port used allready'}),400

    @app.route("/sim/stop",methods=["GET", "POST"])
    @use_args({"port": fields.Str(required=False)}, location="query")

    def stop(args):
        global list_of_threads
        data = request.json

        if(args != {}):
            port = int(args["port"])

        #data will take residence over values in params/args
        if(data != None):
            port = int(data["port"])
        print("stop ",port)
        if port in list_of_threads:
            list_of_webServer[port]['proc'].server_close()
            #list_of_threads[port].join()
            del list_of_webServer[port]
            del list_of_threads[port]
            return (f"Server with port {port } stopped.")
        elif port == CMD_PORT:
            global shutdown
            shutdown=True
            return "Server shutdown", 200
        else:
            return (f"Error: No Server assigned for port = {port}")

    def get_all_rules(port):
        global list_of_rules
        ret_val= []
        for k, v in list_of_rules.items():
            if k == port:
                ret_val.append(v)

        if ret_val == []:
            ret_val = f"\t Empty Rule List"

        return json.dumps(ret_val)
      
    @app.route("/sim/status",methods=["GET", "POST"])
    @use_args({"port": fields.Str(required=True)}, location="query")
    def status(args):
        ret_val= []
        global list_of_threads
        port = int(args["port"])
        for k, v in list_of_threads.items():
            if v.is_alive():
                ret_val.append(f'''"simlinear-server_port": {k},"status":"Running","Rules":"{get_all_rules(port)}"''')

            else:
                ret_val.append (f'''"simlinear-server_port": {k}, "status":"Not Running"''')

        if ret_val == []:
            ret_val = "No server port running"
        return json.dumps(ret_val) , 200

    @app.route("/sim/config2",methods=["POST"])
    def config2():
        #Existing rule approach is a mess. Adding alternative support
        rules.new_rule(request.json)
        print("Success")
        return "Success",200

    @app.route("/sim/config",methods=["GET", "POST"])
    def config():
        global list_of_rules
        try:
            if request :
                #validate params
                if 'GET' in request.method:
                    query_params = normalize_query(request.args)
                    print (request.args.to_dict())

                if 'POST' in request.method:
                    query_params = request.json

                print("config ",query_params)
                if 'port' in query_params:
                    port = int(query_params['port'])
                else:
                    raise Exception("Mandatory parameter 'port' is missing in config request")


                if 'cmd' in query_params:
                    cmd = query_params['cmd']

                    if 'rule_id' in query_params:
                        print ("rule_id")
                        rule_id = query_params['rule_id']
                        #check fragments,assettype and errorcodes
                        #if list_of_webServer

                        if (cmd == 'add') or (cmd == 'edit'):
                            #check if rule_id allready there or new one
                            #res = [ sub['gfg'] for sub in test_list ]

                            #check fragments,assettype and errorcodes
                            if 'assettype' in query_params:
                                assettype = query_params['assettype']
                            else:
                                Exception("Mandatory assettype param missing in config query:" + str(request.args))

                            if 'fragments' in query_params:
                                fragments = query_params['fragments']
                                query_params['fraglist'] = [int(x) for x in fragments.split(',')]
                            else:
                                Exception("Mandatory fragments param missing in config query:" + str(request.args))

                            if 'errorcode' in query_params:
                                errorcode = query_params['errorcode']
                            else:
                                Exception("Mandatory 'errorcode' param missing in config query:" + str(request.args))

                            found_rule = False

                            for i in range(len(list_of_rules)):
                                if list_of_rules[port]['rule_id'] == rule_id:
                                    found_rule = True
                                    break

                            if found_rule != True or query_params['cmd'] == 'edit':
                                query_params['fragment-count']=0
                                list_of_rules[port]=query_params
                                return f"Successfully added rule with rule_id={rule_id}", 200
                            else:
                                Exception(f"Can't add Rule as rule_id={rule_id} allready exist")

                        elif query_params['cmd'] == 'del':
                            print ("del rule")
                            # using del + loop 
                            # to delete dictionary in list
                            found_rule = False
                            for i in range(len(list_of_rules)):
                                if list_of_rules[i]['rule_id'] == rule_id:
                                    del list_of_rules[i]
                                    return f"Successfully Deleted rule with rule_id={rule_id}" , 200
                                    found_rule = True
                                    break
                            
                            if found_rule != True:
                                return f"Invalid del param: no rule exist for parameter rule_id={rule_id} ", 400
                            
                        else:
                                raise Exception(" Invalid 'cmd'= {} value in config request" ,query_params['port'])
                    else:

                        raise Exception("Mandatory parameter 'rule_id' is missing in config request")

                else:
                        raise Exception("Mandatory parameter 'cmd' is missing in config request")
            else:
                return "No query string received", 200 
        except Exception as e:
            e.args
            return e, 400 


    
def run_test():
    
    test_urls = [
                    #'''curl -X POST -d '{"url":""}' http://localhost:5000/harvest   -H 'Content-Type: application/json' ''', 
                    '''curl -X POST -d '{"port":"8085"}' http://localhost:5000/sim/start   -H 'Content-Type: application/json' ''',
                    '''curl http://localhost:5000/sim/status?port=8085 ''',
                    
                    #TO start an HLS session the following cmd should be used
                    #curl -X POST -d '{"port":"8085", "type":"HLS"}' http://localhost:5000/sim/start   -H 'Content-Type: application/json'

                    #'''curl -X POST -d '{"port":8085,"cmd":"add","rule_id":"err405","assettype":"mp4v$","fragments":"10,12,14,40","errorcode":"1000"}' http://localhost:5000/sim/config -H 'Content-Type: application/json' ''',
                    #'''curl -X POST -d '{"port":8085,"cmd":"add","rule_id":"err405","assettype":"mp4v$","fragments":"-1","errorcode":"1000"}' http://localhost:5000/sim/config -H 'Content-Type: application/json' ''',
            #        '''curl -X POST -d '{"port":8085,"cmd":"add","rule_id":"manifest","assettype":"mpd$","fragments":"4,7","errorcode":"404"}' http://localhost:5000/sim/config -H 'Content-Type: application/json' ''',

                    #curl -X POST -d '{"port":8085,"cmd":"add","rule_id":"err404","assettype":"mp4v$","fragments":"6,7,8,9,10,11,12,13,14","errorcode":"404"}' http://localhost:5000/sim/config -H 'Content-Type: application/json' ,
                    #Test Manifest error being introduced at 0,   
      #              " curl http://127.0.0.1:8085/itans.s1e1-ad.mpd",
       #             "curl http://127.0.0.1:8085/itans.s1e1-ad.mpd",
        #              "curl http://127.0.0.1:8085/itans.s1e1-ad.mpd",
         #           "curl http://127.0.0.1:8085/itans.s1e1-ad.mpd",
          #            "curl http://127.0.0.1:8085/itans.s1e1-ad.mpd",
           #         "curl http://127.0.0.1:8085/itans.s1e1-ad.mpd",
            #          "curl http://127.0.0.1:8085/itans.s1e1-ad.mpd",
             #       "curl http://127.0.0.1:8085/itans.s1e1-ad.mpd",
              #      '''curl -X POST -d '{"port":"8085","cmd"="del","rule_id":"err404"}' http://localhost:5000/sim/config -H 'Content-Type: application/json' ''',
               #     '''curl -X POST -d '{"port":"8085"}' http://localhost:5000/sim/start   -H 'Content-Type: application/json' ''' 
                ]
    
    
    time.sleep(1)
    for test in test_urls:
    #for test_dic in test_dic_list:
        print(f"\nExecuting Test URL: {test}")
        result = subprocess.run([test],shell=True, check=True)
        time.sleep(2)
        print (result)
    
    print ("Entering interpreter mode:")
    line =''
    for line in sys.stdin.readline():        
        time.sleep(1)
        if 'q' == line.rstrip():
            break
        print(f'Input : {line}')
        if line != '\n':
            print(f"Executing Test URL: {line}")
            result = subprocess.run(["curl", line])
            print ("\n"+result)

    print("Exiting Simlinear test mode")     
  
if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='Startup server for servicing media streams')
    parser.add_argument('-i', '--interface', help="Interface to listen upon", type=str, default="127.0.0.1")
    args = parser.parse_args()
    hostName = args.interface

    app = Flask(__name__) 
    #mysql = MySQL()
    #app.config['MYSQL_DATABASE_USER'] = 'root'
    #app.config['MYSQL_DATABASE_PASSWORD'] = 'root'
    #app.config['MYSQL_DATABASE_DB'] = 'EmpData'
    #app.config['MYSQL_DATABASE_HOST'] = 'localhost'
    #mysql.init_app(app)
    #executor = Executor(app)
    init_routes() 

    threading.Thread(target=lambda: app.run(host=hostName, port=CMD_PORT, debug=True, use_reloader=False),daemon=True).start()
    
    while shutdown==False:
        time.sleep(1)
    print("Did get shutdown")
    #app.run(debug=True,port = 5001)
   
    #time.sleep(5)
    #print("doing test run")
    #run_test()

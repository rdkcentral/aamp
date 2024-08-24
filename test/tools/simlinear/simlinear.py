#!/usr/bin/env python3
#
# python3 -m simlinear
# simlenear
import os
from pickle import GET
import subprocess
import sys
import logging
import json
import multiprocessing
from ast import List
from http.server import BaseHTTPRequestHandler, HTTPServer
import socket
import threading
import time
import re
from datetime import datetime
import argparse
from flask import Flask, jsonify, request
import webargs
from webargs.flaskparser import use_args
from library.manifests import read_harvest_details, ManifestServerCommon
from collections import defaultdict
from urllib.parse import urlparse, parse_qsl, urlsplit #RDKAAMP-3019
import base64 #RDKAAMP-3019

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

CMD_PORT = 5000

list_of_rules = {}

# {'port':port,'status':'init'/'running'/'stopped','proc':"ref to webserver instance",'process':"ref to webserver process instance",'rules':list_of_rules}
list_of_webServer = {}
list_of_threads = {}
list_of_processes = {}

refreshVal = -1
numOfRequests = 0
# havest_process
shutdown = False
restart = True

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
    """
    Test to see if socket port is available
    """
    a_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    location = (hostName, port)
    result_of_check = a_socket.connect_ex(location)
    a_socket.close()

    if result_of_check == 0:
        log.warning("Port is getting used")
        return False

    log.info("Port is not used")
    return True

def display_all_manifests(host, port, abr_type):
    """
    To display all available manifests
    """
    dashFiles = 0
    hlsFiles = 0
    manifest_dict = defaultdict(int)
    hostInfo = "http://" + host + ':' + str(port) + '/'

    if(abr_type == "DASH"):
        ext = ".mpd"
    elif(abr_type == "HLS"):
        ext = ".m3u8"

    filelist = []
    for path, dirs, files in os.walk('.'):
        if path == "./tmp":
            continue
        for fileName in files:
            if ((fileName not in filelist) and (ext in fileName)):
                filelist.append(os.path.relpath(path + "/"+ fileName))
    
    print("Serving manifests")
    for file in filelist:
        file_path = os.path.dirname(file).lstrip(".").lstrip("/")
        fileName = os.path.basename(file)
        
        if (("._" not in fileName) and (fileName.endswith(ext))):
            print(hostInfo + file_path + "/" + fileName)

        if (("._" not in fileName) and (ext+'.' in fileName) and (".orig" not in fileName)):
            content = fileName.split(".")
            try:
                manifest_dict[content[0]] = manifest_dict.get(content[0]) + 1
            except:
                manifest_dict[content[0]] = 1

    for manifest in manifest_dict.keys():
        if(manifest_dict[manifest] > 1):
            print(hostInfo + file_path + "/" + manifest + ext + " [1 - " + str(manifest_dict[manifest]) + ']')
        else:
            print(hostInfo + file_path + "/" + manifest + ext + "." + str(manifest_dict[manifest]))

def modify_response(path): #RDKAAMP-3019
    qParams = dict(parse_qsl(urlsplit(path).query))
    if qParams.get("respData","") != "":
        params = json.loads(base64.b64decode(qParams.get("respData","")))
        for param in params[::-1]:
            if int(param.get('delay', 0)) > 0 and re.findall(param.get('pattern', ""), path):
                if int(param.get('delay', 0)) <= 10000:
                    time.sleep(float(param.get('delay'))/1000)
                else:
                    log.info(f"Delay limit exceeded {param.get('delay', 0)}, Limit is 10000 for URI {path}")
            if int(param.get('status', 200)) == 404 and re.findall(param.get('pattern', ""), path):
                raise FileNotFoundError
            if int(param.get('status', 200)) == 500 and re.findall(param.get('pattern', ""), path):
                return 'An error occurred, please check logs for details.', 500

#################################################################
class DASHServer(ManifestServerCommon):
    """
    Methods specific to DASH and persistent data
    """

    harvest_to_playback_delta = None
    is_live_time_based_manifest = None  # Not decided to start with

    def dash_update_mpd_for_live_time(self, path):
        """
        For a manifest with type="dynamic"  media="..$TIME$.mp4" then
        $Time$ = fn( time_now, availabilityStartTime )

        Playback will occur in the future I.E after harvesting so the
        calculated values of $Time$ will be different for playback
        hence the segments filenames will be different to those harvested.

        We update the availabilityStartTime field to compensate for a
        different value of time_now during playback:
        new_availabilityStartTime = availabilityStartTime
        + ( start_time_of_playback -time_of_harvest )

        Only simlinear.py knows start_time_of_playback because it will
        be the time when simlinear.py starts serving the manifest.  simlinear
        will need to modify the manifests as it serves them.
        """
        with open(path, "r") as f:
            contents = f.read()

        if self.is_live_time_based_manifest is False:
            # Already decided this is not live time based
            return contents

        if self.is_live_time_based_manifest is None:
            # First manifest fetch, need to decide what type it is.
            # We are looking for the following:
            # <MPD ... type="dynamic" ...>
            # ...
            # <SegmentTemplate ... media="index_video_2_0_$Time$.mp4" ...>

            is_live_time = re.search(
                r'type="dynamic".*\$Time\$', contents, flags=re.DOTALL
            )
            if is_live_time:
                # Calculate how we need to update the manifest
                self.is_live_time_based_manifest = True
                details = read_harvest_details()
                recording_start_time = details["recording_start_time"]
                when_recorded = datetime.fromisoformat(recording_start_time[:19])
                playback_start = datetime.utcnow()
                self.harvest_to_playback_delta = playback_start - when_recorded
                log.info("Decided is live timebase")
            else:
                self.is_live_time_based_manifest = False
                # Not a manifest type that needs updating
                # do nothing
                log.info("Decided not live timebase")
                return contents

        # Now update the availabilityStartTime field by adding on harvest_to_playback_delta
        re_pattern = r'{}="(.*?)"'.format("availabilityStartTime")
        pattern_match = re.search(re_pattern, contents)
        if pattern_match:
            ast = datetime.fromisoformat(pattern_match.group(1)[:19])
            ast += self.harvest_to_playback_delta
            ast_str = ast.isoformat()
            log.info(
                "availabilityStartTime %s updated to %s",
                    pattern_match.group(1), ast_str
            )
            contents = re.sub(
                r'(?<={}=").*?(?=")'.format("availabilityStartTime"), ast_str, contents
            )

        else:
            log.error("Cannot find %s in %s", re_pattern, path)
            sys.exit(1)
        return contents

    def dash_get_manifest(self, base):
        """
        Find dash manifest to serve and potentially modify as needed
        """
        log.info("dash_get_manifest %s",base)
        rtn_path = self.manifest_serve(base)

        if rtn_path:
            content = self.dash_update_mpd_for_live_time(rtn_path)
            return {"path": rtn_path, "contents": content}

        # Failed to find suitable manifest
        return None


#####################################################
dash_server = DASHServer()


class DASHServerHandler(BaseHTTPRequestHandler):
    """
    Handler for DASH http requests
    """

    def do_GET(self):
        """
        Handler for http get
        """
        
        global numOfRequests
        global refreshVal
        global list_of_threads
        global restart
        global shutdown
        # path=/some/kind/of/path?query
        # becomes some/kind/of/path
        path = self.path[1:].split("?")[0]

        if self.path.endswith(".m3u8"):
            log.error("ERROR This looks like a HLS request but I am a DASH server")
            sys.exit(1)

        try:
            # if self.path.endswith(".mpd"): #RDKAAMP-3019
            if path.endswith(".mpd"): #RDKAAMP-3019
                if refreshVal > 0:
                    numOfRequests += 1
                    print()
                    print("Request:", numOfRequests)
                    if numOfRequests == refreshVal:
                        port = list(list_of_threads.keys())[0]
                        self.send_response(408)
                        self.end_headers()
                        
                        self.connection.close()
                        
                        restart = True
                        shutdown = True
                        
                        return;
                rtn = dash_server.dash_get_manifest(path)
                if not rtn:
                    raise FileNotFoundError
                log.info("%s %s",time.time(), rtn["path"])
            else:
                rtn = {"path": path}

            if "contents" in rtn:
                #RDKAAMP-1435
                details = read_harvest_details()
                if details != {}:
                    if details['url'] is not None:
                        baseURLs = re.findall(r'<BaseURL>([\S]*)<\/BaseURL>', rtn["contents"])
                        if len(baseURLs) > 0:
                            for baseURL in baseURLs:
                                basURL_domain = urlparse(baseURL)
                                if basURL_domain.netloc in details['url']:
                                    rtn["contents"] = re.sub(r'<BaseURL>https?://'+basURL_domain.netloc, f"<BaseURL>http://{self.server.server_address[0]}:{self.server.server_address[1]}/{basURL_domain.netloc}", rtn["contents"])
                                elif args.ad_server != "":
                                    # Replace BaseURL to Ad-Server
                                    rtn["contents"] = re.sub(r'<BaseURL>https?://'+basURL_domain.netloc, f"<BaseURL>{args.ad_server}/{basURL_domain.netloc}", rtn["contents"])

                contents = rtn["contents"].encode("utf-8")
            else:
                modify_response(self.path) #RDKAAMP-3019
                    
                with open(rtn["path"], "rb") as f:
                    contents = f.read()

            self.send_response(200)
            self.send_header("Access-Control-Allow-Origin", "*")

            self.end_headers()
            self.wfile.write(contents)
        except FileNotFoundError:
            self.send_response(404)
            self.end_headers()


###############################################################################
class HLSServer(ManifestServerCommon):
    """
    HTTP server specialisation for serving up HLS based content
    """
    pass

#####################################################
hls_server = HLSServer()


class HLSServerHandler(BaseHTTPRequestHandler):
    """
    Instance of this class only exists for handling of 1 http GET
    hence the existance of hls_server which persists for the whole
    streaming session and can therefore hold state information for
    that session
    """

    def do_GET(self):
        """
        Handler for http get
        """
        
        global numOfRequests
        global refreshVal
        global list_of_threads
        global restart
        global shutdown
        
        # path = self.path[1:] #RDKAAMP-3019
        path = self.path[1:].split("?")[0] #RDKAAMP-3019

        if self.path.endswith(".mpd"):
            log.error("ERROR This looks like a DASH request but I am a HLS server")
            sys.exit(1)

        try:
            if ".m3u8" in self.path:
                if refreshVal > 0:
                    numOfRequests += 1
                    print()
                    print("Request:", numOfRequests)
                    if numOfRequests == refreshVal:
                        port = list(list_of_threads.keys())[0]
                        self.send_response(408)
                        self.end_headers()
                        
                        self.connection.close()
                        
                        restart = True
                        shutdown = True
                        
                        return;
                rtn_path = hls_server.manifest_serve(path)
                if not rtn_path:
                    raise FileNotFoundError
                rtn = {"path": rtn_path}
            else:
                rtn = {"path": path}  # Serving a segment

            if "contents" in rtn:
                contents = rtn["contents"]
            else:
                modify_response(self.path) #RDKAAMP-3019
                
                with open(rtn["path"], "rb") as f:
                    contents = f.read()

            self.send_response(200)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(contents)
        except FileNotFoundError:
            self.send_response(404)
            self.end_headers()


#################################################################


def start_web_server(port, abr_type):
    """
    Start DASH/HLS server on specified port
    """
    global list_of_webServer

    if port in list_of_webServer:
        log.error(f"A server is already assigned for port = {port}")
        return
    elif abr_type == "HLS":
        # Start a HLS server
        list_of_webServer[port] = {
            "status": "init",
            "proc": HTTPServer((hostName, port), HLSServerHandler),
            "process": None,
        }
        log.info("HLS Server started http://%s:%s",hostName, port)
        display_all_manifests(hostName, port, abr_type)
    else:
        # Start a DASH server
        list_of_webServer[port] = {
            "status": "init",
            "proc": HTTPServer((hostName, port), DASHServerHandler),
            "process": None,
        }
        log.info("Server started http://%s:%s",hostName, port)
        display_all_manifests(hostName, port, abr_type)

    try:
        list_of_webServer[port]["proc"].serve_forever()
    except KeyboardInterrupt:
        pass

    list_of_webServer[port]["proc"].server_close()
    del list_of_webServer[port]
    log.info("Server stopped.")


def stop_web_server(port):
    global list_of_threads
    global list_of_webServer

    if port in list_of_threads:
        list_of_webServer[port]["proc"].server_close()
        list_of_threads[port].join()
        del list_of_webServer[port]
        del list_of_threads[port]


def init_routes():
    @app.route("/sim/start", methods=["GET", "POST"])
    @use_args({"port": webargs.fields.Str(required=False)}, location="query")
    def start(args):
        global list_of_threads
        data = request.json

        if args != {}:
            port = int(args["port"])

        # data will take residence over values in params/args
        if data != None:
            port = int(data["port"])

        abr_type = data.get("type", "DASH")

        if is_port_available(port):
            global list_of_webServer

            thread1 = threading.Thread(
                target=lambda: start_web_server(port, abr_type), daemon=True
            )
            thread1.isDaemon = True
            list_of_threads[port] = thread1
            list_of_threads[port].start()
            return jsonify({"result": "success"}), 200
        else:
            return jsonify({"result": "fail- port used already"}), 400

    @app.route("/sim/stop", methods=["GET", "POST"])
    @use_args({"port": webargs.fields.Str(required=False)}, location="query")
    def stop(args):
        global list_of_threads
        data = request.json

        if args != {}:
            port = int(args["port"])

        # data will take residence over values in params/args
        if data != None:
            port = int(data["port"])
        log.info("stop %s", port)
        if port in list_of_threads:
            list_of_webServer[port]["proc"].server_close()
            # list_of_threads[port].join()
            del list_of_webServer[port]
            del list_of_threads[port]
            return f"Server with port {port } stopped."
        elif port == CMD_PORT:
            global shutdown
            shutdown = True
            return "Server shutdown", 200
        else:
            return f"Error: No Server assigned for port = {port}"

    def get_all_rules(port):
        global list_of_rules
        ret_val = []
        for k, v in list_of_rules.items():
            if k == port:
                ret_val.append(v)

        if ret_val == []:
            ret_val = f"\t Empty Rule List"

        return json.dumps(ret_val)

    @app.route("/sim/status", methods=["GET", "POST"])
    @use_args({"port": webargs.fields.Str(required=True)}, location="query")
    def status(args):
        ret_val = []
        global list_of_threads
        port = int(args["port"])
        for k, v in list_of_threads.items():
            if v.is_alive():
                ret_val.append(
                    f'''"simlinear-server_port": {k},"status":"Running","Rules":"{get_all_rules(port)}"'''
                )

            else:
                ret_val.append(
                    f'''"simlinear-server_port": {k}, "status":"Not Running"'''
                )

        if ret_val == []:
            ret_val = "No server port running"
        return json.dumps(ret_val), 200

    @app.route("/sim/config2", methods=["POST"])
    def config2():
        # Existing rule approach is a mess. Adding alternative support
        rules.new_rule(request.json)
        log.info("Success")
        return "Success", 200

    @app.route("/sim/config", methods=["GET", "POST"])
    def config():
        global list_of_rules
        try:
            if request:
                # validate params
                if "GET" in request.method:
                    query_params = normalize_query(request.args)
                    log.info("%s",request.args.to_dict())

                if "POST" in request.method:
                    query_params = request.json

                log.info("config %s", query_params)
                if "port" in query_params:
                    port = int(query_params["port"])
                else:
                    raise Exception(
                        "Mandatory parameter 'port' is missing in config request"
                    )

                if "cmd" in query_params:
                    cmd = query_params["cmd"]

                    if "rule_id" in query_params:
                        log.info("rule_id")
                        rule_id = query_params["rule_id"]
                        # check fragments,assettype and errorcodes
                        # if list_of_webServer

                        if (cmd == "add") or (cmd == "edit"):
                            # check if rule_id allready there or new one
                            # res = [ sub['gfg'] for sub in test_list ]

                            # check fragments,assettype and errorcodes
                            if "assettype" in query_params:
                                assettype = query_params["assettype"]
                            else:
                                Exception(
                                    "Mandatory assettype param missing in config query:"
                                    + str(request.args)
                                )

                            if "fragments" in query_params:
                                fragments = query_params["fragments"]
                                query_params["fraglist"] = [
                                    int(x) for x in fragments.split(",")
                                ]
                            else:
                                Exception(
                                    "Mandatory fragments param missing in config query:"
                                    + str(request.args)
                                )

                            if "errorcode" in query_params:
                                errorcode = query_params["errorcode"]
                            else:
                                Exception(
                                    "Mandatory 'errorcode' param missing in config query:"
                                    + str(request.args)
                                )

                            found_rule = False

                            for i in range(len(list_of_rules)):
                                if list_of_rules[port]["rule_id"] == rule_id:
                                    found_rule = True
                                    break

                            if found_rule != True or query_params["cmd"] == "edit":
                                query_params["fragment-count"] = 0
                                list_of_rules[port] = query_params
                                return (
                                    f"Successfully added rule with rule_id={rule_id}",
                                    200,
                                )
                            else:
                                Exception(
                                    f"Can't add Rule as rule_id={rule_id} allready exist"
                                )

                        elif query_params["cmd"] == "del":
                            log.info("del rule")
                            # using del + loop
                            # to delete dictionary in list
                            found_rule = False
                            for i in range(len(list_of_rules)):
                                if list_of_rules[i]["rule_id"] == rule_id:
                                    del list_of_rules[i]
                                    return (
                                        f"Successfully Deleted rule with rule_id={rule_id}",
                                        200,
                                    )
                                    found_rule = True
                                    break

                            if found_rule != True:
                                return (
                                    f"Invalid del param: no rule exist for parameter rule_id={rule_id} ",
                                    400,
                                )

                        else:
                            raise Exception(
                                " Invalid 'cmd'= {} value in config request",
                                query_params["port"],
                            )
                    else:
                        raise Exception(
                            "Mandatory parameter 'rule_id' is missing in config request"
                        )

                else:
                    raise Exception(
                        "Mandatory parameter 'cmd' is missing in config request"
                    )
            else:
                return "No query string received", 200
        except Exception as e:
            e.args
            return e, 400


def run_test():
    test_urls = [
        #'''curl -X POST -d '{"url":""}' http://localhost:5000/harvest   -H 'Content-Type: application/json' ''',
        """curl -X POST -d '{"port":"8085"}' http://localhost:5000/sim/start   -H 'Content-Type: application/json' """,
        """curl http://localhost:5000/sim/status?port=8085 """,
        # TO start an HLS session the following cmd should be used
        # curl -X POST -d '{"port":"8085", "type":"HLS"}' http://localhost:5000/sim/start   -H 'Content-Type: application/json'
        #'''curl -X POST -d '{"port":8085,"cmd":"add","rule_id":"err405","assettype":"mp4v$","fragments":"10,12,14,40","errorcode":"1000"}' http://localhost:5000/sim/config -H 'Content-Type: application/json' ''',
        #'''curl -X POST -d '{"port":8085,"cmd":"add","rule_id":"err405","assettype":"mp4v$","fragments":"-1","errorcode":"1000"}' http://localhost:5000/sim/config -H 'Content-Type: application/json' ''',
        #        '''curl -X POST -d '{"port":8085,"cmd":"add","rule_id":"manifest","assettype":"mpd$","fragments":"4,7","errorcode":"404"}' http://localhost:5000/sim/config -H 'Content-Type: application/json' ''',
        # curl -X POST -d '{"port":8085,"cmd":"add","rule_id":"err404","assettype":"mp4v$","fragments":"6,7,8,9,10,11,12,13,14","errorcode":"404"}' http://localhost:5000/sim/config -H 'Content-Type: application/json' ,
        # Test Manifest error being introduced at 0,
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
        # for test_dic in test_dic_list:
        print(f"\nExecuting Test URL: {test}")
        result = subprocess.run([test], shell=True, check=True)
        time.sleep(2)
        print(result)

    print("Entering interpreter mode:")
    line = ""
    for line in sys.stdin.readline():
        time.sleep(1)
        if "q" == line.rstrip():
            break
        print(f"Input : {line}")
        if line != "\n":
            print(f"Executing Test URL: {line}")
            result = subprocess.run(["curl", line])
            print("\n" + result)

    print("Exiting Simlinear test mode")

logging.basicConfig(
    format="%(funcName)-15s:%(lineno)04d %(message)s",
    stream=sys.stdout,
)
log = logging.getLogger("root")
log.setLevel(logging.INFO)


def duration_to_seconds(duration_str):
    parts = duration_str.split(":")
    
    if len(parts) == 3:  # Format: hours:minutes:seconds
        hours, minutes, seconds = map(int, parts)
        total_seconds = hours * 3600 + minutes * 60 + seconds
    elif len(parts) == 2:  # Format: minutes:seconds
        minutes, seconds = map(int, parts)
        total_seconds = minutes * 60 + seconds
    elif len(parts) == 1:  # Format: seconds
        total_seconds = int(parts[0])
    else:
        raise ValueError("Invalid duration format")

    return total_seconds



if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Startup server for servicing media streams"
    )
    parser.add_argument(
        "-i",
        "--interface",
        help="Interface to listen upon",
        type=str,
        default="127.0.0.1",
    )
    parser.add_argument(
        "-p", "--port", help="Control interface port (default " + str(CMD_PORT) + ")", type=int, default=CMD_PORT
    )
    parser.add_argument(
        "--dash", help="Direct start of DASH server on specified port with no control interface", type=int
    )
    parser.add_argument(
        "--ad_server", help="Ad-Server URL", type=str, default="" #RDKAAMP-1435
    )
    parser.add_argument(
        "--hls", help="Direct start of HLS server on specified port with no control interface", type=int
    )
    parser.add_argument(
        "--refresh", help="after Nth manifest fresh, send HTTP 408 and restart simlinear", type=int
    )
    parser.add_argument(
        "--offset", help="offset time for dash to skip, offset should be in hours:minutes:seconds or minutes:seconds or seconds.", default="0", type=str
    )

    args = parser.parse_args()
    hostName = args.interface

    CMD_PORT=args.port

    app = Flask(__name__)

    details = read_harvest_details()
    if details != {}:
        print("Found Harvest Details")
    else:
        print("WARNING: missing harvest details, playback may be impacted.")
    
    init_routes()
    
    while restart == True:
    
        restart = False
      
        if args.refresh:
            refreshVal = args.refresh

        if args.hls:
            list_of_threads[args.hls] = threading.Thread(
                target=lambda: start_web_server(args.hls, "HLS"), daemon=True
            )
            list_of_threads[args.hls].start()
        elif args.dash:
            if args.offset:
                time_offset = duration_to_seconds(args.offset)
                dash_server.set_offset_time(time_offset)
                list_of_threads[args.dash] = threading.Thread(
                target=lambda: start_web_server(args.dash, "DASH"), daemon=True
            )
            list_of_threads[args.dash].start()
        else:
            threading.Thread(
                target=lambda: app.run(
                    host=hostName, port=CMD_PORT, debug=True, use_reloader=False
                ),
                daemon=True,
            ).start()


        while shutdown == False:
            time.sleep(1)
        if restart:
            if args.hls:
                #list_of_threads[args.hls].join()
                del list_of_threads[args.hls]
                list_of_webServer[args.hls]["proc"].server_close()
                del list_of_webServer[args.hls]
                
            elif args.dash:
                #list_of_threads[args.dash].join()
                del list_of_threads[args.dash]
                list_of_webServer[args.dash]["proc"].server_close()
                del list_of_webServer[args.dash]
            shutdown = False
            time.sleep(5)
            print("Restarting...")
        else:
            print("Did get shutdown")
    # app.run(debug=True,port = 5001)

    # time.sleep(5)
    # print("doing test run")
    # run_test()

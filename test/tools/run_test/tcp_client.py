#!/usr/bin/env python3
#Connects to GStreamer "tcpserversink" in AAMP simulator and looks for gaps in audio/video output
#Simultaneously reads AAMP simulator log lines from STDIN and writes to STDOUT adding in
#extra log messages when A/V gap detected.

import socket
import select
import time
import sys
import fcntl
import os

IS_STREAMING_UNKNOWN=0
IS_STREAMING_STOPPED=1
IS_STREAMING_RUNNING=2

data =[
{ "fd": None , "name":"audio", "port":6124, "time_of_last_read":0, "is_streaming":IS_STREAMING_UNKNOWN },
{ "fd": None , "name":"video", "port":6123, "time_of_last_read":0, "is_streaming":IS_STREAMING_UNKNOWN }
]

server_connected = False
last_try_time=0
server_connect_tries=4
GAP_TIME_SEC=1

###############################################
def log(msg):
    """
    STD format for log messages created by this code.
    """
    print("{:10.3f}: {} {}".format(time.time(),sys.argv[0],msg))

def connect_to_tcp_server():
    """
    Connect to the ports on the gstreamer tcpserversink that
    exists in the AAMP simulator.
    """
    global poller
    global server_connected
    global last_try_time
    global data
    global server_connect_tries
    if server_connected==False and time.time() > (last_try_time+5) and server_connect_tries>0:
        try:
            log("Trying to connect")
            for d in data:
                d["fd"] = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                d["fd"].connect(('127.0.0.1', d["port"]))
                poller.register(d["fd"], select.POLLIN)
                d["time_of_last_read"] = time.time()
            log("Connected to TCP server")
            server_connected = True
        except Exception as e:
            log(e)
            last_try_time = time.time()
            server_connect_tries -=1
            return

def handle_server_poll(fd):
    """
    Return True if EOf is indicated.
    """
    eof = False
    global data
    for d in data:
        file_no = d["fd"].fileno()
        if fd == file_no:
            rx, address = d["fd"].recvfrom(8192)
            l =len(rx)
            if l == 0:
                eof = True
            else:
                #Check for A/V streaming starting again
                t=time.time()
                if d["is_streaming"] == IS_STREAMING_STOPPED:
                    gap = t - d["time_of_last_read"]
                    if gap>GAP_TIME_SEC:
                        log("Streaming {} gap {}".format(d["name"],gap))
                    log("Streaming {} started".format(d["name"]))
                d["is_streaming"] = IS_STREAMING_RUNNING
                d["time_of_last_read"]=t

    return eof

aamp_log_lines=""
def handle_aamp_poll():
    """
    Reads in chunk of aamp log lines from stdin.
    Writes to stdout but each chunk of data must end with "\n"
    because this script can insert it's own log messages and we
    done want the inserted log message getting put in the middle of
    an AAMP log line.
    """
    global aamp_log_lines
    line = sys.stdin.read(2048)
    aamp_log_lines += line
    if line == "" or aamp_log_lines.find("AAMPGstPlayer_Stop")>0:
        print(aamp_log_lines)
        sys.stdout.flush()
        sys.exit()
    last = aamp_log_lines.rfind("cmd: ")
    if last == -1:
        last = aamp_log_lines.rfind("\n")
    if last != -1:
        print(aamp_log_lines[:last+1],end="")
        sys.stdout.flush()
        aamp_log_lines= aamp_log_lines[last+1:]

orig_fl = fcntl.fcntl(sys.stdin, fcntl.F_GETFL)
fcntl.fcntl(sys.stdin, fcntl.F_SETFL, orig_fl | os.O_NONBLOCK)

poller = select.poll()

poller.register(sys.stdin, select.POLLIN)
eof =False
while not eof:
    evts = poller.poll(50)
    #print(evts)
    for fd, evt in evts:
        if evt & select.POLLIN:
            if fd == sys.stdin.fileno():
                handle_aamp_poll()
            if server_connected == True:
                if handle_server_poll(fd) == True:
                    eof = True


    #Try to connect to the tcpsercer or do nothing it already connected.
    connect_to_tcp_server()

    #Check for gaps in streaming >1sec
    n = time.time()-GAP_TIME_SEC
    for d in data:
        if n>d["time_of_last_read"] and d["is_streaming"] == IS_STREAMING_RUNNING:
            log("Streaming {} stopped".format(d["name"]))
            d["is_streaming"] = IS_STREAMING_STOPPED

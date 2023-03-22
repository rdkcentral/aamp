#!/usr/bin/env python3
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia Ltd.
#
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Package up and send commands to the ABR proxy
import requests
import socket

class ProxyCtrl:

    def __init__(self):
        """
        Obtain the required ip addresses, neeed to access the proxy command interface with its ip, and not name.
        Obtain the session cookies, set all comunication to go through the proxy, and set the headers to include the _xsrf token.
        """
        self.proxy_s_ip = socket.gethostbyname('abrtestproxy')
        self.aamp_ip = socket.gethostbyname('aamp')

        self.proxy_s = proxy_s = requests.Session()
        proxy_s.proxies.update({'http': 'http://abrtestproxy:8080', 'https': 'http://abrtestproxy:8080'})
        response = proxy_s.get('http://{}:8081'.format(self.proxy_s_ip))
        if '_xsrf' in proxy_s.cookies:
            proxy_s.headers.update({'X-XSRFToken': proxy_s.cookies['_xsrf']})
        else:
            print("ERROR : No _xsrf token found")
            self.xsrf = ""

    def SetRate(self, rate_k_bits_s):
        """
        Throttle the max bitrate
        """
        payload = {"arguments":["eth0",f"{self.aamp_ip}",f"{rate_k_bits_s}"]}
        r = self.proxy_s.post(f'http://{self.proxy_s_ip}:8081/commands/traffic.setrate', json=payload)
        print("Set rate {} response: {} payload {} ".format(rate_k_bits_s, r, r.json()))

    def ErrorReply(self, token, rule, err_code, ntimes):
        """
        Set a rule to return the response_code to any matching requests
        """
        payload = {"arguments":[f"{token}", f"{rule}", err_code, ntimes]}
        r = self.proxy_s.post(f'http://{self.proxy_s_ip}:8081/commands/rules.errorreply', json=payload)
        print("Error reply  {} {} {} {} response: {} payload {}".format(token, rule, err_code, ntimes, r, r.json()))

    def RemoveRule(self, token):
        """
        Remove the rule associated with token
        """
        payload = {"arguments":[f"{token}"]}
        r = self.proxy_s.post(f'http://{self.proxy_s_ip}:8081/commands/rules.remove', json=payload)
        print("Remove rule {} response: {} payload {}".format(token, r, r.json()))




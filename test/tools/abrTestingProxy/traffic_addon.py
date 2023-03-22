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

import subprocess

from mitmproxy import command

class Traffic:
    def __init__(self):
        self.rate_set = False;
        print("Traffic init")

    def execute_tc_cmd(self, cmd):
        print(cmd)
        res = subprocess.run([cmd], capture_output=True, text=True, shell=True)
        assert(res.stderr == ""), res.stderr

    @command.command("traffic.clearrate")
    def clearrate(self, dev: str) -> str:
        print(f"Clear Rate {dev}")
        try:
            self.execute_tc_cmd(f"tc qdisc del dev {dev} root")
        except AssertionError as error:
            return "{}".format(error)
        self.rate_set = False
        return "OK"

    @command.command("traffic.setrate")
    def setrate(self, dev: str, ip_address: str, kbitrate: str) -> str:
        #dev probably not needed as in docker it's ethn by default where n starts as 0
        print("Set Rate {} {} {}".format(dev, ip_address, kbitrate))
        if self.rate_set == False:
            self.rate_set = True
            try:
                self.execute_tc_cmd("tc qdisc add dev {} root handle 1: htb default 30".format(dev))
                self.execute_tc_cmd("tc class add dev {} parent 1: classid 1:1 htb rate {}kbit ceil {}kbit".format(dev, kbitrate, kbitrate))
                self.execute_tc_cmd("tc filter add dev {} parent 1: protocol ip prio 10 u32 match ip dst {} flowid 1:1".format(dev, ip_address))
                return "OK"
            except AssertionError as error:
                return "{}".format(error)

        else:
            try:
                self.execute_tc_cmd("tc class change dev {} parent 1: classid 1:1 htb rate {}kbit ceil {}kbit".format(dev, kbitrate, kbitrate))
                return "OK"
            except AssertionError as error:
                return "{}".format(error)

addons = [Traffic()]

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

import re
from mitmproxy import command
from mitmproxy import http

response_rules = {}

class Rules:
    def __init__(self):
        response_rules = {}
        print("Rules init")

    @command.command("rules.remove")
    def remove(self, token: str) -> str:
        del response_rules[token]
        return "OK"

    @command.command("rules.errorreply")
    def errorreply(self, token: str, rule: str, err_code: int, ntimes: int) -> str:
        print("Adding error reply rule {} {} {} {}".format(token, rule, err_code, ntimes))
        response_rules[token] = (rule, err_code, ntimes)
        print(f"Current rules: {response_rules}")
        return "OK"

def request(flow: http.HTTPFlow) -> None:
    # If many rules are active it maybe worth concatenating them into
    # one regex as an initial gating check as most urls probably won't
    # match any rule
    for key, (rule, err_code, ntimes) in response_rules.items():
        if re.match(rule, flow.request.pretty_url):
            ntimes -= 1
            print("returning {} for request URL {} due to rule {} ".format(err_code, flow.request.pretty_url, response_rules[key] ))
            flow.response = http.Response.make( err_code )

            if ntimes == 0:
                del response_rules[key]
            else:
                response_rules[key] = (rule, err_code, ntimes)
            print(f"Current rules: {response_rules}")
            break

addons = [Rules()]

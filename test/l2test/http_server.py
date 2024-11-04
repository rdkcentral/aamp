# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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

import http.server
import socketserver
import threading

Handler = http.server.SimpleHTTPRequestHandler

class ReusableTCPServer(socketserver.TCPServer):
    # This allows the server to reuse the same address if a previous HTTP server socket
    # is in the process of closing (SO_REUSEADDR)
    allow_reuse_address = True

class HttpServerThread(threading.Thread):
    def __init__(self, port):
        super().__init__()
        self.port = port
        self.httpd = ReusableTCPServer(("127.0.0.1", self.port), Handler)

    def run(self):
        print(f"Serving files at {self.httpd.server_address}")
        self.httpd.serve_forever()

    def stop(self):
        print("Stopping server...")
        self.httpd.shutdown()
        self.httpd.server_close()  # This closes the server socket

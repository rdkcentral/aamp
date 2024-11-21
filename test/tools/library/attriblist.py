#!/usr/bin/env python3
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 RDK Management
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
import os
import sys
from datetime import *
import time


class AttribList(object):
    """
    Class to parse a line containing name=value parameters into
    attributes for the class instance. If supplied with a dictionary,
    then it will use this as the source of the name/value pairs.
    Dictionaries within dictionaries will create a recusive structure.
    """

    def __init__(self, src, start=None, sep=",", to_upper=True):
        self._sep = sep

        if type(src) is str:
            self._quoted = set()

            if start:
                src = src[src.index(start) + 1 :]
            i = 0

            while i < len(src):
                end = src[i:].index("=") + i
                if end < i:
                    raise ValueError("Missing '=' from name on %s" % src)

                name = src[i:end].upper() if to_upper else src[i:end]
                name = name.strip()
                i = end + 1

                if src[i] == '"':
                    i += 1
                    end = src[i:].index('"') + i
                    setattr(self, name, src[i:end])

                    i = end + 1
                    self._quoted.add(name)

                    if i >= len(src):
                        break
                    if src[i] != sep:
                        raise ValueError("Missing separator %s in %s" % (sep, src))

                    i += 1
                    self._quoted.add(name)

                else:
                    end = src[i:].find(sep) + i

                    if end < i:
                        setattr(self, name, src[i:])
                        break

                    setattr(self, name, src[i:end])
                    i = end + 1

        elif isinstance(src, AttribList):
            self.__dict__ = vars(src)

        elif type(src) is dict:
            self._quoted = None

            for name, val in src.items():
                if val is dict:
                    setattr(self, name, AttribList(val))
                else:
                    setattr(self, name, val)

        else:
            raise ValueError("Bad type %s passed to AttribList constructor" % type(src))

    def merge(self, src):
        """
        Merge in a set of attributes from another instance.
        """
        if isinstance(src, AttribList):
            self.__dict__.update(vars(src))

        else:
            self._quoted = None

            for name, val in src.items():
                if val is dict:
                    setattr(self, name, NameList(val))
                else:
                    setattr(self, name, val)

        return self

    def intattr(self, name, default=None):
        """
        Convert an attribute into an 'int' with an option default.
        """
        if hasattr(self, name):
            try:
                num = int(getattr(self, name))
            except:
                Exception("Mandatory value with name %s is not an int" % name)

        elif default is None:
            return False

        else:
            num = default

        setattr(self, name, num)
        return True

    def load_prefix(self, from_dict, prefix):
        """
        Load in attributes from a dictionary that have names with the specified
        prefix.
        """
        for n, v in from_dict:
            if n.startswith(prefix):
                setattr(self, n, v)

        return self

    def __str__(self):
        """
        Return list of variables back as a string.
        """
        sep = ""
        res = ""

        for name, val in vars(self).items():
            if name.startswith("_"):
                continue

            if self._quoted is None or name in self._quoted:
                res += sep + name + '="' + str(val) + '"'
            else:
                res += sep + name + "=" + str(val)

            sep = self._sep

        return res


if __name__ == "__main__":
    base_dir = sys.argv[0]
    base_dir = base_dir[: base_dir.rfind("/")]

    time.sleep(1)
    print("no tests at the moment")


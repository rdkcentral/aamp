#!/bin/bash

set -o errexit
set -o pipefail
set -o nounset
# set -o xtrace

MITMPROXY_PATH="/home/mitmproxy/.mitmproxy"

if [ -f "$MITMPROXY_PATH/mitmproxy-ca.pem" ]; then
  f="$MITMPROXY_PATH/mitmproxy-ca.pem"
else
  f="$MITMPROXY_PATH"
fi
usermod -o \
    -u $(stat -c "%u" "$f") \
    -g $(stat -c "%g" "$f") \
    mitmproxy

exec gosu mitmproxy mitmweb --web-host 0.0.0.0 -s traffic_addon.py -s rules_addon.py

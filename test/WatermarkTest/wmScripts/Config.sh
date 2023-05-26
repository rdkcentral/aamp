#!/bin/bash
# Setup surface and plugin for displaying watermarks - note this may not be required on some platforms
systemctl stop sky-appsservice sky-asplayer.service;
curl -X POST -H "Content-Type: application/json" 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method": "Controller.1.activate", "params": { "callsign": "org.rdk.RDKShell" }}';
echo;
curl -X POST -H "Content-Type: application/json" 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method": "org.rdk.RDKShell.1.createDisplay", "params": {"client": "org.rdk.Watermark",   "callsign": "org.rdk.Watermark",  "displayName": "as-watermark", "displayWidth": 1920,  "displayHeight": 1080, "virtualDisplay": true, "virtualWidth": 1920, "virtualHeight": 1080, "topmost": false, "focus": false}}';
echo;
curl -X POST -H "Content-Type: application/json" 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method": "Controller.1.deactivate", "params": { "callsign": "org.rdk.Watermark" }}';
echo;
curl -X POST -H "Content-Type: application/json" 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method": "Controller.1.configuration@org.rdk.Watermark", "params": {"clientidentifier":"as-watermark"}}'
echo;
curl -X POST -H "Content-Type: application/json" 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method": "Controller.1.activate", "params": { "callsign": "org.rdk.Watermark" }}';
echo;


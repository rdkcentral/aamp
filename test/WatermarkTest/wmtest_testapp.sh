# update ip addreass &
# run on device to start test app
systemctl stop sky-appsservice sky-asplayer.service;
curl -X POST -H "Content-Type: application/json" 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method": "Controller.1.activate", "params": { "callsign": "org.rdk.RDKShell" }}';
echo;
curl -X POST -H "Content-Type: application/json" 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method": "Controller.1.deactivate", "params": { "callsign": "org.rdk.Watermark" }}';
echo;
curl -X POST -H "Content-Type: application/json" 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method": "Controller.1.activate", "params": { "callsign": "org.rdk.Watermark" }}';
echo;
curl -X POST -H "Content-Type: application/json" 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method": "org.rdk.RDKShell.1.destroy", "params": {"callsign":"htmlapp1"}}';
echo;
curl -X POST -H "Content-Type: application/json" 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method": "org.rdk.RDKShell.1.launch", "params": {"callsign":"htmlapp1","type": "HtmlApp","uri":"http://10.169.1.102:80/test.html"}}';
echo;
journalctl -f|egrep -i  "watermark|getAndCallback|htmlapp1|error";

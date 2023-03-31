systemctl stop sky-appsservice sky-asplayer.service;
curl -X POST -H "Content-Type: application/json" 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method": "Controller.1.activate", "params": { "callsign": "org.rdk.RDKShell" }}';
echo;
curl -X POST -H "Content-Type: application/json" 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method": "org.rdk.RDKShell.1.launch", "params": {"callsign":"htmlapp1","type": "HtmlApp","uri":"http://10.169.1.102:80/aamponly.html"}}';
echo;
curl -X POST -H "Content-Type: application/json" 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method": "Controller.1.deactivate", "params": { "callsign": "org.rdk.Watermark" }}';
echo;
curl -X POST -H "Content-Type: application/json" 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method": "Controller.1.activate", "params": { "callsign": "org.rdk.Watermark" }}';
echo;
curl -d '{"jsonrpc":"2.0","id":"4","method": "org.rdk.Watermark.1.showWatermark", "params": {"show":true}}' http://127.0.0.1:9998/jsonrpc;
echo;
curl -d '{"jsonrpc":"2.0","id":"4","method": "org.rdk.Watermark.1.createWatermark", "params": {"id":4661, "zorder":1}}' http://127.0.0.1:9998/jsonrpc;
echo;
/usr/bin/WatermarkTestClient 46 /opt/wmimage.png&
sleep 1;
curl -d '{"jsonrpc":"2.0","id":"4","method": "org.rdk.Watermark.1.updateWatermark", "params": {"id":4661, "key":46, "size":375879}}' http://127.0.0.1:9998/jsonrpc;
echo;
journalctl -f|egrep "watermark";

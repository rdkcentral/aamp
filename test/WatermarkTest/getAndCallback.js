function getAndCallback(url, metadata, Callback) {
    var request = new XMLHttpRequest();
    request.open('GET', url, true);
    request.responseType = 'blob';
    request.onprogress = function() {
        console.log("progress " + String((this.loaded/this.total)*100) + "%")
    }
    request.onreadystatechange = function() {

        console.log("onreadystatechange " + this.statusText + " " + String(this.readyState))
    }
    request.onload = function() {
        console.log("response successful")
        var fr = new FileReader()
        fr.readAsArrayBuffer(request.response)
        fr.onload  = function() {
            console.log("got array buffer of " + String(fr.result.byteLength) + " bytes")
            Callback(fr.result, metadata)
        }
    }

    request.send()
}
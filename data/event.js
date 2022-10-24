if (!!window.EventSource) {
    var source = new EventSource('/events');

    source.addEventListener('open', function (e) {
        console.log("Events Connected");
    }, false);
    source.addEventListener('error', function (e) {
        if (e.target.readyState != EventSource.OPEN) {
            console.log("Events Disconnected");
        }
    }, false);

    source.addEventListener('message', function (e) {
        console.log("message", e.data);
    }, false);

    source.addEventListener('new_readings', function (e) {
        console.log("new_readings", e.data);
        var obj = JSON.parse(e.data);
        document.getElementById("t" + obj.id).innerHTML = obj.temperature.toFixed(1);
        document.getElementById("h" + obj.id).innerHTML = obj.humidity;
        document.getElementById("rt" + obj.id).innerHTML = obj.readingId;
        document.getElementById("rh" + obj.id).innerHTML = obj.readingId;
    }, false);

    source.addEventListener('new_device_state', function (e) {
        console.log("new_device_state", e.data);
        var obj1 = JSON.parse(e.data);
        document.getElementById("state" + obj1.id).innerHTML = obj1.state;
    }, false);
}
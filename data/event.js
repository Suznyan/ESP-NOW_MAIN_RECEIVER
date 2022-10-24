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

    source.addEventListener('b2new_readings', function (e) {
        console.log("board2_new_readings", e.data);
        var obj2 = JSON.parse(e.data);
        document.getElementById("t" + obj2.id).innerHTML = obj2.temperature.toFixed(1);
        document.getElementById("h" + obj2.id).innerHTML = obj2.humidity;
        document.getElementById("r" + obj2.id).innerHTML = obj2.readingId;
    }, false);

    source.addEventListener('b3new_reading', function (e) {
        console.log("board3_new_reading", e.data);
        var obj3 = JSON.parse(e.data);
        document.getElementById("t" + obj3.id).innerHTML = obj3.temperature.toFixed(2);
        document.getElementById("r" + obj3.id).innerHTML = obj3.readingId;
    }, false);

    source.addEventListener('new_device_state', function (e) {
        console.log("new_device_state", e.data);
        var obj1 = JSON.parse(e.data);
        document.getElementById("state" + obj1.id).innerHTML = obj1.state;
    }, false);
}
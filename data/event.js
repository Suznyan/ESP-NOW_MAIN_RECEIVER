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
        document.getElementById("state" + obj2.id).innerHTML = obj2.state;
    }, false);

    source.addEventListener('b3new_reading', function (e) {
        console.log("board3_new_reading", e.data);
        var obj3 = JSON.parse(e.data);
        document.getElementById("t" + obj3.id).innerHTML = obj3.temperature.toFixed(2);
        document.getElementById("r" + obj3.id).innerHTML = obj3.readingId;
        document.getElementById("state" + obj3.id).innerHTML = obj3.state;
    }, false);

    source.addEventListener('new_device_state', function (e) {
        console.log("new_device_state", e.data);
        var obj1 = JSON.parse(e.data);
        document.getElementById("state" + obj1.id).innerHTML = obj1.state;
    }, false);
}

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    // websocket.onmessage = onMessage; // <-- add this line
}
function onOpen(event) {
    console.log('Connection opened');
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

// function onMessage(event) {
//     var state;
//     if (event.data == "1") {
//         state = "ON";
//     }
//     else {
//         state = "OFF";
//     }
//     document.getElementById('state').innerHTML = state;
// }

window.addEventListener('load', onLoad);

function onLoad(event) {
    initWebSocket();
    initButton();
}

function initButton() {
    document.getElementById('button1').addEventListener('click', toggle1);
    document.getElementById('button2').addEventListener('click', toggle2);
    document.getElementById('button3').addEventListener('click', toggle3);
    document.getElementById('button4').addEventListener('click', toggle4);
}

function toggle1() {
    websocket.send('toggle1');
}

function toggle2() {
    websocket.send('toggle2');
}

function toggle3() {
    websocket.send('toggle3');
}

function toggle4() {
    websocket.send('toggle4');
}
function onSocketOpen()
{
    console.log("onSocketOpen");
    this._socket.send(
        "OPTIONS * WEBRTSP/0.1\r\n" +
        "CSeq: 1\r\n");
}

function onSocketClose(event)
{
    console.log(event.code, event.reason);
}

function onSocketError(error)
{
    console.log(error.message);
}

function onSocketMessage(event)
{
    console.log(event.data);
}

function connect(url)
{
    if(this._socket) {
        this._socket.close();
        this._socket = null;
    }

    this._socket = new WebSocket(url, "webrtsp");

    this._socket.onopen = () => this._onSocketOpen();
    this._socket.onclose = (event) => this._onSocketClose(event);
    this._socket.onerror = (error) => this._onSocketError(error);
    this._socket.onmessage = (event) => this._onSocketMessage(event);
}

export function WebRTSP()
{
    this._socket = null;
    this._onSocketOpen = onSocketOpen;
    this._onSocketClose = onSocketClose;
    this._onSocketError = onSocketError;
    this._onSocketMessage = onSocketMessage;

    this.connect = connect;
}

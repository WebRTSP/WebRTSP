import Session from "./RtspSession.mjs"
import * as Serialize from "./RtspSerialize.mjs"
import * as Parse from "./RtspParse.mjs"
import * as StatusCode from "./RtspStatusCode.mjs"

class ClientSession extends Session
{

constructor(sendRequest, sendResponse, videoElement) {
    super(sendRequest, sendResponse);

    this._uri = "http://example.com/";
    this._video = videoElement;
    this._session = null;
    this._peerConnection = new RTCPeerConnection();

    let pc = this._peerConnection;
    pc.ontrack =
        (event) => { this._onTrack(event); };
    pc.onicecandidate =
        (event) => { this._onIceCandidate(event); };
}

onConnected()
{
    this.requestDescribe(this._uri);
}

handleMessage(message)
{
    if(Parse.IsRequest(message)) {
        const request = Parse.ParseRequest(message);
        if(!request) {
            this.disconnect();
            return;
        }

        if(!this.handleRequest(request)) {
            this.disconnect();
            return;
        }
    } else {
        const response = Parse.ParseResponse(message);
        if(!response) {
            this.disconnect();
            return;
        }

        if(!this.handleResponse(response)) {
            this.disconnect();
            return;
        }
    }
}

onDescribeResponse(request, response)
{
    console.log("onDescribeResponse");

    if(StatusCode.OK != response.statusCode)
        return false;

    if(!response.session)
        return false;

    this._session = response.session;

    const offer = response.body;
    const promise =
        this._peerConnection.setRemoteDescription(
            { type : "offer", sdp : offer });
    promise
        .then(() => {
            console.log("setRemoteDescription complete");
            this._sendAnswer();
        }).
        catch((event) => {
            console.log("setRemoteDescription fail", event);
        });

    return true;
}

handleSetupRequest(request)
{
    console.log("handleSetupRequest");

    const contentType = request.headerFields.get("content-type");
    if(!contentType || contentType != "application/x-ice-candidate")
        return false;

    const iceCandidate = request.body;
    if(!iceCandidate)
        return false;

    const separatorIndex = iceCandidate.indexOf("/");
    if(separatorIndex == -1 || separatorIndex == 0)
        return false;

    const eolIndex = iceCandidate.indexOf("\r\n", separatorIndex);
    if(eolIndex == -1 || eolIndex == 0)
        return false;

    const sdpMLineIndex = iceCandidate.substring(0, separatorIndex);
    const candidate = iceCandidate.substring(separatorIndex + 1, eolIndex);

    if("a=end-of-candidates" == candidate) {
        const promise =
            this._peerConnection.addIceCandidate( { sdpMLineIndex, candidate: "" });
        promise
            .then(() => {
                console.log("addIceCandidate complete");
            }).
            catch((event) => {
                console.log("addIceCandidate fail", event);
            });
    } else {
        const promise =
            this._peerConnection.addIceCandidate( { sdpMLineIndex, candidate } );
        promise
            .then(() => {
                console.log("addIceCandidate complete");
            }).
            catch((event) => {
                console.log("addIceCandidate fail", event);
            });
    }

    this.sendOkResponse(request.session);

    return true;
}

onSetupResponse(request, response)
{
    console.log("onSetupResponse");

    if(StatusCode.OK != response.statusCode)
        return false;

    const contentType = request.headerFields.get("Content-Type");
    if(contentType == "application/sdp")
        this.requestPlay(this._uri, this._session)

    return true;
}

onPlayResponse(request, response)
{
    console.log("onPlayResponse");

    if(StatusCode.OK != response.statusCode)
        return false;

    return true;
}


async _sendAnswer()
{
    const answer =
        await this._peerConnection.createAnswer()
            .catch(function (event) {
                console.log("createAnswer fail", event);
            });

    await this._peerConnection.setLocalDescription(answer)
        .catch(function (event) {
            console.log("createAnswer fail", event);
        });

    await this.requestSetup(this._uri, "application/sdp", this._session, answer.sdp);
}

_onTrack(event)
{
    console.log("_onTrack");

    this._video.srcObject = event.streams[0];
}

async _onIceCandidate(event)
{
    console.log("_onIceCandidate", event.candidate);

    let candidate;
    if(event.candidate && event.candidate.candidate)
        candidate =
            event.candidate.sdpMLineIndex.toString() + "/" + event.candidate.candidate + "\r\n";
    else
        candidate =
            "0/a=end-of-candidates\r\n";

    await this.requestSetup(
        this._uri,
        "application/x-ice-candidate",
        this._session,
        candidate);
}

}

export class WebRTSP
{

constructor(videoElement) {
    this._socket = null;
    this._session = null;
    this._video = videoElement;
}

_onSocketOpen()
{
    console.log("onSocketOpen");
    this._session =
        new ClientSession(
            (request) => { this._sendRequest(request); },
            (response) => { this._sendResponse(response); },
            this._video
        );

    this._session.onConnected();
}

_onSocketClose(event)
{
    console.log("_onSocketClose", event.code, event.reason);
    this._close();
}

_onSocketError(error)
{
    console.log("_onSocketError", error.message);
    this._close();
}

_onSocketMessage(event)
{
    console.log(event.data);
    this._session.handleMessage(event.data);
}

_close()
{
    this._session = null;

    if(this._socket) {
        this._socket.close();
        this._socket = null;
    }
}

_sendRequest(request)
{
    if(!request) {
        this._close();
        return;
    }

    const requestMessage = Serialize.SerializeRequest(request);

    if(!requestMessage) {
        this._close();
        return;
    }

    this._socket.send(requestMessage);
}

_sendResponse(response)
{
    if(!response) {
        this._close();
        return;
    }

    const responseMessage = Serialize.SerializeResponse(response);

    if(!responseMessage) {
        this._close();
        return;
    }

    this._socket.send(responseMessage);
}

connect(url)
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

}

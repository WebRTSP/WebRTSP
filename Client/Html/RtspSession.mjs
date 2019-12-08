import * as Protocol from "./RtspProtocol.mjs"
import * as Method from "./RtspMethod.mjs"
import * as StatusCode from "./RtspStatusCode.mjs"
import Request from "./RtspRequest.mjs"
import Response from "./RtspResponse.mjs"

export default class Session
{

constructor(sendRequest, sendResponse) {
    this._sendRequest = sendRequest;
    this._sendResponse = sendResponse;

    this._nextCSeq = 1;
    
    this._sentRequests = new Map();
}

disconnect()
{
    this._sendRequest(undefined);
}

createRequest(method, uri, session)
{
    for(;this._sentRequests.has(this._nextCSeq); ++this._nextCSeq);

    let request = new Request();
    request.method = method;
    request.protocol = Protocol.WEBRTSP_0_1;
    request.cseq = this._nextCSeq;
    request.uri = uri;

    if(session)
        request.session = session;
    
    this._sentRequests.set(this._nextCSeq, request);

    return request;
}

requestOptions(uri)
{
    let request = this.createRequest(Method.OPTIONS, uri);
    this._sendRequest(request);

    return request.cseq;
}

requestSetup(uri, contentType, session, body)
{
    let request = this.createRequest(Method.SETUP, uri);

    if(session)
        request.session = session;

    request.headerFields.set("Content-Type", contentType);

    request.body = body;

    this._sendRequest(request);
}

requestDescribe(uri)
{
    let request = this.createRequest(Method.DESCRIBE, uri);

    this._sendRequest(request);

    return request.cseq;
}

requestPlay(uri, session)
{
    let request = this.createRequest(Method.PLAY, uri, session);

    this._sendRequest(request);

    return request.cseq;
}

requestTeardown(uri, session)
{
    let request = this.createRequest(Method.TEARDOWN, uri, session);

    this._sendRequest(request);

    return request.cseq;
}

createResponse(statusCode, reasonPhrase, cseq, session)
{
    let response = new Response();
    response.protocol = Protocol.WEBRTSP_0_1;
    response.statusCode = statusCode;
    response.reasonPhrase = reasonPhrase;
    response.cseq = cseq;

    if(session)
        response.headerFields.emplace("Session", session);

    return response;
}

sendOkResponse(cseq, session)
{
    const response = this.createResponse(StatusCode.OK, "OK", cseq, session);
    this._sendResponse(response);
}

handleRequest(request)
{
    switch(request.method) {
    case Method.SETUP:
        if(this.handleSetupRequest)
            return this.handleSetupRequest(request);
        else
            return false;
    default:
        return false;
    }
}

handleResponse(response)
{
    const request = this._sentRequests.get(response.cseq);
    if(!request)
        return false;

    this._sentRequests.delete(response.cseq);

    switch(request.method) {
        case Method.OPTIONS:
            if(this.onOptionsResponse)
                return this.onOptionsResponse(request, response);
            else
                return false;
        case Method.DESCRIBE:
            if(this.onDescribeResponse)
                return this.onDescribeResponse(request, response);
            else
                return false;
        case Method.SETUP:
            if(this.onSetupResponse)
                return this.onSetupResponse(request, response);
            else
                return false;
        case Method.PLAY:
            if(this.onPlayResponse)
                return this.onPlayResponse(request, response);
            else
                return false;
        case Method.TEARDOWN:
            if(this.onTeardownResponse)
                return this.onTeardownResponse(request, response);
            else
                return false;
        default:
            return false;
    }
}

}

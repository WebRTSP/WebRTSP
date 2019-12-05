import * as Protocol from "./RtspProtocol.mjs"
import * as Method from "./RtspMethod.mjs"

function SerializeStatusCode(statusCode)
{
    if(statusCode > 999)
        return "999";
    else if(statusCode < 100)
        return "100";
    else
        return statusCode.toString();
}

export function SerializeRequest(request)
{
    let out = Method.Name(request.method);
    out += " ";
    out += request.uri;
    out += " ";
    out += Protocol.Name(request.protocol);
    out += "\r\n";

    out += "CSeq: ";
    out += request.cseq.toString();
    out += "\r\n";

    for(let [key, value] of request.headerFields) {
        out += key;
        out += ": ";
        out += value;
        out += "\r\n";
    }

    if(request.body) {
        out +="\r\n";
        out += request.body;
    }

    return out;
}

export function SerializeResponse(response)
{
    let out = Protocol.Name(response.protocol);
    out += " ";
    out += SerializeStatusCode(response.statusCode);
    out += " ";
    out += response.reasonPhrase;
    out += "\r\n";

    out += "CSeq: ";
    out += response.cseq.toString();
    out += "\r\n";

    for(let [key, value] of response.headerFields) {
        out += key;
        out += ": ";
        out += value;
        out += "\r\n";
    }

    if(response.body) {
        out +="\r\n";
        out += response.body;
    }

    return out;
}

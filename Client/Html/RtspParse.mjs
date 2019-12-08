import Token from "./Token.mjs"
import ParseBuffer from "./ParseBuffer.mjs"
import * as Protocol from "./RtspProtocol.mjs"
import * as Method from "./RtspMethod.mjs"
import Request from "./RtspRequest.mjs"
import Response from "./RtspResponse.mjs"

function IsWSP(c)
{
    if(c.length != 1)
        return false;

    return c == ' ' || c == '\t';
}

function IsCtl(c)
{
    if(c.length != 1)
        return false;

    const code = c.charCodeAt(0);
    return (code >= 0 && code <= 31) || code == 127;
}

function IsDigit(c)
{
    if(c.length != 1)
        return false;

    switch(c) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return true;
    }

    return false;
}

function ParseDigit(c)
{
    if(c.length != 1)
        return undefined;

    switch(c) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return c.charCodeAt(0) - '0'.charCodeAt(0);
    default:
        return undefined;
    }
}

function IsTspecials(c)
{
    if(c.length != 1)
        return false;

    switch(c) {
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ',':
    case ';':
    case ':':
    case '\\':
    case '"':
    case '/':
    case '[':
    case ']':
    case '?':
    case '=':
    case '{':
    case '}':
    case ' ':
    case '\t':
        return true;
    }

    return false;
}

function SkipWSP(parseBuffer)
{
    let savePos = parseBuffer.pos;

    for(; parseBuffer.pos < parseBuffer.length && IsWSP(parseBuffer.currentChar); parseBuffer.advance());

    return savePos != parseBuffer.pos;
}

function SkipEOL(parseBuffer)
{
    switch(parseBuffer.currentChar) {
    case '\n':
        parseBuffer.advance();
        return true;
    case '\r':
        parseBuffer.advance();
        if(!parseBuffer.eos && parseBuffer.currentChar == '\n')
            parseBuffer.advance();
        return true;
    }

    return false;
}

function SkipFolding(parseBuffer)
{
    let tmpParseBuffer = parseBuffer.clone();

    if(!SkipEOL(tmpParseBuffer))
        return false;
    if(!SkipWSP(tmpParseBuffer))
        return false;

    parseBuffer.assign(tmpParseBuffer);
    return true;
}

function SkipLWS(parseBuffer)
{
    let tmpParseBuffer = parseBuffer.clone();

    SkipEOL(tmpParseBuffer);
    if(!SkipWSP(tmpParseBuffer))
        return false;

    parseBuffer.assign(tmpParseBuffer);

    return true;
}

function Skip(parseBuffer, c)
{
    if(parseBuffer.eos)
        return false;

    if(parseBuffer.currentChar == c) {
        parseBuffer.advance();
        return true;
    }

    return false;
}

function GetToken(parseBuffer)
{
    let token = new Token(parseBuffer);

    for(; !parseBuffer.eos; parseBuffer.advance()) {
        if(IsCtl(parseBuffer.currentChar) || IsTspecials(parseBuffer.currentChar))
            break;
    }

    token.length = parseBuffer.pos - token.pos;
    if(!token.empty)
        return token;
    else
        return undefined;
}

function GetProtocol(parseBuffer)
{
    let token  = new Token(parseBuffer);

    const protocolName = "WEBRTSP";
    const protocolNameLength = protocolName.length;

    if(parseBuffer.tailLength < protocolNameLength + 4)
        return undefined;

    if(!parseBuffer.startsWith(protocolName))
        return undefined;
    parseBuffer.advance(protocolNameLength);

    if(parseBuffer.currentChar != '/')
        return undefined;
    parseBuffer.advance();

    if(!IsDigit(parseBuffer.currentChar))
        return undefined;
    parseBuffer.advance();

    if(parseBuffer.currentChar != '.')
        return undefined;
    parseBuffer.advance();

    if(!IsDigit(parseBuffer.currentChar))
        return undefined;
    parseBuffer.advance();

    token.length = parseBuffer.pos - token.pos;

    return token;
}

function GetURI(parseBuffer)
{
    // FIXME! fix according to rfc

    let uriToken = new Token(parseBuffer);

    for(; !parseBuffer.eos; parseBuffer.advance()) {
        if(IsCtl(parseBuffer.currentChar) || parseBuffer.currentChar == ' ')
            break;
    }

    uriToken.length = parseBuffer.pos - uriToken.pos;

    if(!uriToken.empty)
        return uriToken;
    else
        return undefined;
}

function ParseMethodLine(parseBuffer, request)
{
    const methodToken = GetToken(parseBuffer);
    if(!methodToken)
        return false;

    request.method = Method.Parse(methodToken);
    if(!request.method)
        return false;

    if(!SkipWSP(parseBuffer))
        return false;

    const uriToken = GetURI(parseBuffer);
    if(!uriToken)
        return false;
    request.uri = uriToken.string;

    if(!SkipWSP(parseBuffer))
        return false;

    const protocolToken = GetProtocol(parseBuffer);
    if(!protocolToken)
        return false;

    request.protocol = Protocol.Parse(protocolToken);
    if(!request.protocol)
        return false;

    if(!SkipEOL(parseBuffer))
        return false;

    return true;
}

function ParseHeaderField(parseBuffer, headerFields)
{
    const nameToken = GetToken(parseBuffer);
    if(nameToken.empty)
        return false;

    if(!Skip(parseBuffer, ':'))
        return false;

    SkipLWS(parseBuffer);

    const valueToken = new Token(parseBuffer);

    while(!parseBuffer.eos) {
        const tmpPos = parseBuffer.pos;
        if(SkipFolding(parseBuffer))
            continue;
        else if(SkipEOL(parseBuffer)) {
            const lowerName = nameToken.string.toLowerCase();
            valueToken.length = tmpPos - valueToken.pos;

            headerFields.set(lowerName, valueToken.string);

            return true;
        } else if(!IsCtl(parseBuffer.currentChar))
            parseBuffer.advance();
        else
            return false;
    }

    return false;
}

function ParseCSeq(token, out)
{
    let cseq = 0;

    for(let c of token) {
        if(!IsDigit(c))
            return false;

        const digit = ParseDigit(c);

        if(cseq > (cseq * 10 + digit)) {
            // overflow
            return false;
        }

        cseq = cseq * 10 + digit;
    }

    if(!cseq)
        return false;

    if(out)
        out.cseq = cseq;

    return true;
}

export function ParseRequest(buffer)
{
    let parseBuffer = new ParseBuffer(buffer);
    let request = new Request;

    if(!ParseMethodLine(parseBuffer, request))
        return undefined;

    while(!parseBuffer.eos) {
        if(!ParseHeaderField(parseBuffer, request.headerFields))
            return undefined;
        if(SkipEOL(parseBuffer))
            break;
    }

    if(!parseBuffer.eos)
        request.body = parseBuffer.tail;

    const cseqValue = request.headerFields.get("cseq");
    if(!cseqValue)
        return undefined;

    if(!ParseCSeq(cseqValue, request))
        return undefined;

    request.headerFields.delete("cseq");

    const sessionValue = request.headerFields.get("session");
    if(sessionValue) {
        request.session = sessionValue;
        request.headerFields.delete("session");
    }

    return request;
}

function GetStatusCode(parseBuffer)
{
    let token = new Token(parseBuffer);

    if(parseBuffer.tailLength < 3)
        return undefined;

    for(let i = 0; i < 3 && !parseBuffer.eos; ++i, parseBuffer.advance())
        if(!IsDigit(parseBuffer.currentChar))
            return undefined;

    token.length = 3;

    return token;
}

function ParseStatusCode(token)
{
    if(token.empty || token.length < 3)
        return 0;

    return ParseDigit(token.charAt(0)) * 100 +
        ParseDigit(token.charAt(1)) * 10 +
        ParseDigit(token.charAt(2)) * 1;
}

function GetReasonPhrase(parseBuffer)
{
    let token = new Token(parseBuffer);

    for(; !parseBuffer.eos; parseBuffer.advance()) {
        if(IsCtl(parseBuffer.currentChar))
            break;
    }

    token.length = parseBuffer.pos - token.pos;

    return token;
}

function ParseStatusLine(parseBuffer, response)
{
    let protocolToken = GetProtocol(parseBuffer);
    if(!protocolToken || protocolToken.empty)
        return false;

    response.protocol = Protocol.Parse(protocolToken);
    if(!response.protocol)
        return false;

    if(!SkipWSP(parseBuffer))
        return false;

    const statusCodeToken = GetStatusCode(parseBuffer);
    if(!statusCodeToken)
        return false;

    response.statusCode = ParseStatusCode(statusCodeToken);

    if(!SkipWSP(parseBuffer))
        return false;

    const reasonPhrase = GetReasonPhrase(parseBuffer);
    if(reasonPhrase.empty)
        return false;

    response.reasonPhrase = reasonPhrase.string;

    if(!SkipEOL(parseBuffer))
        return false;

    return true;
}

export function ParseResponse(buffer)
{
    let parseBuffer = new ParseBuffer(buffer);
    let response = new Response;

    if(!ParseStatusLine(parseBuffer, response))
        return undefined;

    while(!parseBuffer.eos) {
        if(!ParseHeaderField(parseBuffer, response.headerFields))
            return undefined;
        if(SkipEOL(parseBuffer))
            break;
    }

    if(!parseBuffer.eos)
        response.body = parseBuffer.tail;

    const cseqValue = response.headerFields.get("cseq");
    if(!cseqValue)
        return undefined;

    if(!ParseCSeq(cseqValue, response))
        return undefined;

    response.headerFields.delete("cseq");

    const sessionValue = response.headerFields.get("session");
    if(sessionValue) {
        response.session = sessionValue;
        response.headerFields.delete("session");
    }

    return response;
}

export function IsRequest(buffer)
{
    let parseBuffer = new ParseBuffer(buffer);
    let request = new Request;

    const methodToken = GetToken(parseBuffer);
    if(!methodToken)
        return false;

    request.method = Method.Parse(methodToken);
    if(!request.method)
        return false;

    return true;
}

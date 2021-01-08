#include "RtspParser.h"

#include <cstddef>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <functional>

#include "Methods.h"
#include "Protocols.h"
#include "Token.h"


namespace rtsp {

static inline bool IsEOS(size_t pos, size_t size)
{
    return pos == size;
}

static inline bool IsWSP(char c)
{
    return c == ' ' || c == '\t';
}

static inline bool IsCtl(char c)
{
    return (c >= 0 && c <= 31) || c == 127;
}

static inline bool IsDigit(char c)
{
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

static inline unsigned ParseDigit(char c)
{
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
        return c - '0';
    default:
        return 0;
    }
}

static inline bool IsTspecials(char c)
{
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
    case '\"':
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


static bool IsChar(const char* buf, size_t pos, size_t size, char c)
{
    return !IsEOS(pos, size) && buf[pos] == c;
}

static bool SkipWSP(const char* buf, size_t* pos, size_t size)
{
    const size_t savePos = *pos;

    for(; *pos < size && IsWSP(buf[*pos]); ++(*pos));

    return savePos != *pos;
}

static bool IsEOL(const char* buf, size_t pos, size_t size)
{
    return IsChar(buf, pos, size, '\r') && IsChar(buf, pos + 1, size, '\n');
}

static bool SkipEOL(const char* buf, size_t* pos, size_t size)
{
    if(!IsEOL(buf, *pos, size))
        return false;

    *pos += 2;

    return true;
}

static bool SkipFolding(const char* buf, size_t* pos, size_t size)
{
    size_t tmpPos = *pos;

    if(!SkipEOL(buf, &tmpPos, size))
        return false;
    if(!SkipWSP(buf, &tmpPos, size))
        return false;

    *pos = tmpPos;
    return true;
}

static bool SkipLWS(const char* buf, size_t* pos, size_t size)
{
    size_t tmpPos = *pos;

    SkipEOL(buf, &tmpPos, size);
    if(!SkipWSP(buf, &tmpPos, size))
        return false;

    *pos = tmpPos;
    return true;
}

static bool Skip(const char* buf, size_t* pos, size_t size, char c)
{
    if(IsChar(buf, *pos, size, c)) {
        ++(*pos);
        return true;
    }

    return false;
}

static bool SkipNot(const char* buf, size_t* pos, size_t size, char c)
{
    while(!IsEOS(*pos, size)) {
        if(IsChar(buf, *pos, size, c))
            return true;

        ++(*pos);
    }

    return false;
}

static Token GetToken(const char* buf, size_t* pos, size_t size)
{
    const size_t tokenPos = *pos;

    for(; *pos < size; ++(*pos)) {
        if(IsCtl(buf[*pos]) || IsTspecials(buf[*pos]))
            break;
    }

    Token token;
    if((*pos - tokenPos) > 0) {
        token.token = buf + tokenPos;
        token.size = *pos - tokenPos;
    }

    assert(
        (token.token == nullptr && token.size == 0) ||
        (token.token != nullptr && token.size > 0));

    return token;
}

static Token GetProtocol(const char* buf, size_t* pos, size_t size)
{
    Token token;

    const char protocolName[] = "WEBRTSP";
    const unsigned protocolNameLength = sizeof(protocolName) - 1;

    if(size - *pos < protocolNameLength + 4)
        return token;

    if(0 != strncmp(buf + *pos, protocolName, protocolNameLength))
        return token;

    if(buf[*pos + protocolNameLength] != '/')
        return token;

    if(!IsDigit(buf[*pos + protocolNameLength + 1]))
        return token;

    if(buf[*pos + protocolNameLength + 2] != '.')
        return token;

    if(!IsDigit(buf[*pos + protocolNameLength + 3]))
        return token;

    token.token = buf + *pos;
    token.size = protocolNameLength + 4;

    *pos += token.size;

    return token;
}

static Token GetURI(const char* buf, size_t* pos, size_t size)
{
    // FIXME! fix according to rfc

    const size_t tokenPos = *pos;

    for(; *pos < size; ++(*pos)) {
        if(IsCtl(buf[*pos]) || buf[*pos] == ' ')
            break;
    }

    Token token;
    if((*pos - tokenPos) > 0) {
        token.token = buf + tokenPos;
        token.size = *pos - tokenPos;
    }

    assert(
        (token.token == nullptr && token.size == 0) ||
        (token.token != nullptr && token.size > 0));

    return token;
}

static bool ParseMethodLine(const char* request, size_t* pos, size_t size, Request* out)
{
    const Token methodToken = GetToken(request, pos, size);

    if(IsEmptyToken(methodToken))
        return false;

    out->method = ParseMethod(methodToken);
    if(out->method == Method::NONE)
        return false;

    if(!SkipWSP(request, pos, size))
        return false;

    const Token uri = GetURI(request, pos, size);
    if(IsEmptyToken(uri))
        return false;
    out->uri.assign(uri.token, uri.size);

    if(!SkipWSP(request, pos, size))
        return false;

    const Token protocolToken = GetProtocol(request, pos, size);
    if(IsEmptyToken(protocolToken))
        return false;

    out->protocol = ParseProtocol(protocolToken);
    if(out->protocol == Protocol::NONE)
        return false;

    if(!SkipEOL(request, pos, size))
        return false;

    return true;
}

bool ParseHeaderField(
    const char* buf, size_t* pos, size_t size,
    std::map<std::string, std::string>* headerFields)
{
    const Token name = GetToken(buf, pos, size);
    if(IsEmptyToken(name))
        return false;

    if(!Skip(buf, pos, size, ':'))
        return false;

    SkipLWS(buf, pos, size);

    size_t valuePos = *pos;

    while(*pos < size) {
        size_t tmpPos = *pos;
        if(SkipFolding(buf, pos, size))
            continue;
        else if(SkipEOL(buf, pos, size)) {
            std::string lowerName(name.token, name.size);
            std::transform(
                lowerName.begin(), lowerName.end(),
                lowerName.begin(),
                [] (std::string::value_type c) {
                    return std::tolower(static_cast<unsigned char>(c));
                });

            const Token value { buf + valuePos, tmpPos - valuePos };

            headerFields->emplace(
                lowerName,
                std::string(value.token, value.size));

            return true;
        } else if(!IsCtl(buf[*pos]))
            ++(*pos);
        else
            return false;
    }

    return false;
}

bool ParseCSeq(const std::string& token, CSeq* out) noexcept
{
    CSeq tmpOut = 0;

    for(const std::string::value_type& c: token) {
        if(!IsDigit(c))
            return false;

        unsigned digit = ParseDigit(c);

        if(tmpOut > (tmpOut * 10 + digit)) {
            // overflow
            return false;
        }

        tmpOut = tmpOut * 10 + digit;
    }

    if(!tmpOut)
        return false;

    if(out)
        *out = tmpOut;

    return true;
}

bool ParseRequest(const char* request, size_t size, Request* out) noexcept
{
    size_t position = 0;

    if(!ParseMethodLine(request, &position, size, out))
        return false;

    while(!IsEOS(position, size)) {
        if(!ParseHeaderField(request, &position, size, &(out->headerFields)))
            return false;
        if(IsEOS(position, size))
            break;
        if(SkipEOL(request, &position, size))
            break;
    }

    if(!IsEOS(position, size))
        out->body.assign(request + position, size - position);

    auto cseqIt = out->headerFields.find("cseq");
    if(out->headerFields.end() == cseqIt || cseqIt->second.empty())
        return false;

    if(!ParseCSeq(cseqIt->second, &out->cseq))
        return false;

    out->headerFields.erase(cseqIt);

    return true;
}

static Token GetStatusCode(const char* buf, size_t* pos, size_t size)
{
    Token token;

    if(size - *pos < 3)
        return token;

    const size_t statusCodePos = *pos;

    for(unsigned i = 0; i < 3 && *pos < size; ++i, ++(*pos))
        if(!IsDigit(buf[*pos]))
            return token;

    token.token = buf + statusCodePos;
    token.size = 3;

    return token;
}

unsigned ParseStatusCode(const Token& token)
{
    if(IsEmptyToken(token) || token.size < 3)
        return 0;

    return
        ParseDigit(token.token[0]) * 100 +
        ParseDigit(token.token[1]) * 10 +
        ParseDigit(token.token[2]) * 1;
}

static Token GetReasonPhrase(const char* response, size_t* pos, size_t size)
{
    const size_t reasonPhrasePos = *pos;

    for(; *pos < size; ++(*pos)) {
        if(IsCtl(response[*pos]))
            break;
    }

    return Token{ response + reasonPhrasePos, *pos - reasonPhrasePos };
}

static bool ParseStatusLine(const char* response, size_t* pos, size_t size, Response* out)
{
    const Token protocolToken = GetProtocol(response, pos, size);
    if(IsEmptyToken(protocolToken))
        return false;

    out->protocol = ParseProtocol(protocolToken);
    if(out->protocol == Protocol::NONE)
        return false;

    if(!SkipWSP(response, pos, size))
        return false;

    const Token statusCodeToken = GetStatusCode(response, pos, size);
    if(IsEmptyToken(statusCodeToken))
        return false;

    out->statusCode = ParseStatusCode(statusCodeToken);

    if(!SkipWSP(response, pos, size))
        return false;

    const Token reasonPhrase = GetReasonPhrase(response, pos, size);
    if(IsEmptyToken(reasonPhrase))
        return false;

    out->reasonPhrase.assign(reasonPhrase.token, reasonPhrase.size);

    if(!SkipEOL(response, pos, size))
        return false;

    return true;
}

bool ParseResponse(const char* response, size_t size, Response* out) noexcept
{
    size_t position = 0;

    if(!ParseStatusLine(response, &position, size, out))
        return false;

    while(!IsEOS(position, size)) {
        if(!ParseHeaderField(response, &position, size, &(out->headerFields)))
            return false;
        if(SkipEOL(response, &position, size))
            break;
    }

    if(!IsEOS(position, size))
        out->body.assign(response + position, size - position);

    auto cseqIt = out->headerFields.find("cseq");
    if(out->headerFields.end() == cseqIt || cseqIt->second.empty())
        return false;

    if(!ParseCSeq(cseqIt->second, &out->cseq))
        return false;

    out->headerFields.erase(cseqIt);

    return true;
}

bool IsRequest(const char* request, size_t size) noexcept
{
    size_t position = 0;
    const Token methodToken = GetToken(request, &position, size);

    if(IsEmptyToken(methodToken))
        return false;

    if(ParseMethod(methodToken) == Method::NONE)
        return false;

    return true;
}

static bool ParseParameter(
    const char* buf, size_t* pos, size_t size,
    Parameters* parameters)
{
    Token name { buf + *pos };

    if(!SkipNot(buf, pos, size, ':'))
        return false;

    name.size = buf + *pos - name.token;

    if(!name.size)
        return false;

    if(!Skip(buf, pos, size, ':'))
        return false;

    SkipWSP(buf, pos, size);

    size_t valuePos = *pos;

    while(!IsEOS(*pos, size)) {
        size_t tmpPos = *pos;
        if(SkipEOL(buf, pos, size)) {
            const Token value { buf + valuePos, tmpPos - valuePos };

            parameters->emplace(
                std::string(name.token, name.size),
                std::string(value.token, value.size));

            return true;
        } else if(!IsCtl(buf[*pos]))
            ++(*pos);
        else
            return false;
    }

    return false;
}

bool ParseParameters(
    const std::string& body,
    Parameters* parameters) noexcept
{
    const char* buf = body.data();
    size_t size = body.size();
    size_t position = 0;

    while(!IsEOS(position, size)) {
        if(!ParseParameter(buf, &position, size, parameters))
            return false;
    }

    return true;
}

bool ParseParametersNames(
    const std::string& body,
    ParametersNames* names) noexcept
{
    const char* buf = body.data();
    size_t size = body.size();
    size_t pos = 0;

    while(!IsEOS(pos, size)) {
        const Token name = GetToken(buf, &pos, size);
        if(IsEmptyToken(name))
            return false;

        names->emplace(name.token, name.size);

        if(!SkipEOL(buf, &pos, size))
            return false;
    }

    return true;

}

std::set<rtsp::Method> ParseOptions(const Response& response)
{
    std::set<rtsp::Method> returnOptions;

    auto it = response.headerFields.find("public");
    if(response.headerFields.end() == it)
        return returnOptions;

    std::set<rtsp::Method> parsedOptions;

    const std::string& optionsString = it->second;

    const char* buf = optionsString.data();
    size_t size = optionsString.size();
    size_t pos = 0;

    while(!IsEOS(pos, size)) {
        SkipWSP(buf, &pos, size);
        const Token token = GetToken(buf, &pos, size);

        Method method = ParseMethod(token);

        if(Method::NONE == method)
            return returnOptions;

        SkipWSP(buf, &pos, size);

        if(!IsEOS(pos, size) && !Skip(buf, &pos, size, ','))
            return returnOptions;

        parsedOptions.insert(method);
    }

    returnOptions.swap(parsedOptions);

    return returnOptions;
}

}

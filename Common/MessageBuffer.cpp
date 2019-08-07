#include "MessageBuffer.h"

#include <cassert>


MessageBuffer::MessageBuffer()
{
}

MessageBuffer::MessageBuffer(MessageBuffer&& other)
{
    swap(other);
}

const MessageBuffer::Buffer::value_type*
MessageBuffer::data() const
{
    assert(!_buffer.empty());
    if(_buffer.empty())
        return nullptr;

    assert(_buffer.size() > LWS_PRE);
    if(_buffer.size() <= LWS_PRE)
        return nullptr;

    return _buffer.data() + LWS_PRE;
}

MessageBuffer::Buffer::value_type*
MessageBuffer::data()
{
    assert(!_buffer.empty());
    if(_buffer.empty())
        return nullptr;

    assert(_buffer.size() > LWS_PRE);
    if(_buffer.size() <= LWS_PRE)
        return nullptr;

    return _buffer.data() + LWS_PRE;
}

size_t MessageBuffer::size()
{
    assert(!_buffer.empty());
    if(_buffer.empty())
        return 0;

    assert(_buffer.size() > LWS_PRE);
    if(_buffer.size() <= LWS_PRE)
        return 0;

    return  _buffer.size() - LWS_PRE;
}

bool MessageBuffer::onReceive(lws* wsi, void* chunk, size_t chunkSize)
{
    if(_buffer.empty())
        _buffer.resize(LWS_PRE);

    assert(chunkSize > 0);
    if(chunkSize > 0) {
        _buffer.insert(
            _buffer.end(),
            static_cast<Buffer::value_type*>(chunk),
            static_cast<Buffer::value_type*>(chunk) + chunkSize);
    }

    return lws_is_final_fragment(wsi);
}

void MessageBuffer::append(const char* chunk, size_t chunkSize)
{
    if(_buffer.empty())
        _buffer.resize(LWS_PRE);

    if(chunkSize) {
        _buffer.insert(
            _buffer.end(),
            reinterpret_cast<const Buffer::value_type*>(chunk),
            reinterpret_cast<const Buffer::value_type*>(chunk) + chunkSize);
    }
}

bool MessageBuffer::writeAsText(lws* wsi)
{
    return lws_write(wsi, data(), size(), LWS_WRITE_TEXT) >= 0;
}

void MessageBuffer::swap(MessageBuffer& other)
{
    assert(_buffer.empty() || _buffer.size() > LWS_PRE);
    assert(other._buffer.empty() || other._buffer.size() > LWS_PRE);

    _buffer.swap(other._buffer);
}

void MessageBuffer::clear()
{
    _buffer.clear();
}

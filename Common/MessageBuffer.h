#pragma once

#include <string>
#include <vector>

#include <libwebsockets.h>


class MessageBuffer
{
public:
    typedef std::vector<char> Buffer;

    MessageBuffer();
    MessageBuffer(MessageBuffer&&);

    const Buffer::value_type* data() const;
    Buffer::value_type* data();

    bool empty() const
        { return size() == 0; }
    size_t size() const;

    void append(const char*, size_t);
    void assign(const char*, size_t);
    void assign(const char*);
    void assign(const std::string&);

    bool onReceive(lws*, void* chunk, size_t);
    bool writeAsText(lws*);

    void swap(MessageBuffer& other);

    void clear();

private:
    Buffer _buffer;
};

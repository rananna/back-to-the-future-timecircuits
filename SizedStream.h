#pragma once

#include <Stream.h>

// A simple Stream wrapper that limits the number of bytes that can be read.
// This is useful for parsing a response body with a known Content-Length,
// preventing the parser from reading into the next response on a keep-alive
// connection or from reading garbage data.
class SizedStream : public Stream {
private:
    Stream& _stream;
    size_t _remaining_len;

public:
    SizedStream(Stream& s, size_t len)
        : _stream(s), _remaining_len(len) {}

    virtual int available() {
        // Return the smaller of the underlying stream's available bytes
        // or the remaining length we're allowed to read.
        return min((size_t)_stream.available(), _remaining_len);
    }

    virtual int read() {
        // If we've read the entire specified length, stop.
        if (_remaining_len == 0) {
            return -1;
        }

        int val = _stream.read();
        if (val != -1) {
            // We successfully read a byte, so decrement our remaining length.
            _remaining_len--;
        } else {
            // The underlying stream ended before we read the full length.
            // Set remaining length to 0 to signal we are done.
            _remaining_len = 0;
        }
        return val;
    }

    virtual int peek() {
        if (_remaining_len == 0) {
            return -1;
        }
        // Peek doesn't consume the byte, so we don't decrement _remaining_len.
        return _stream.peek();
    }

    virtual void flush() {
        _stream.flush();
    }

    virtual size_t write(uint8_t data) {
        // Not implemented for this use case.
        return 0;
    }
};
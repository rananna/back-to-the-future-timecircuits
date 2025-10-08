/**
 * @file SizedStream.h
 * @brief Defines the SizedStream class, a wrapper for enforcing read limits on Stream objects.
 */
#pragma once

#include <Stream.h>

/**
 * @class SizedStream
 * @brief A Stream wrapper that limits the number of bytes that can be read from an underlying stream.
 * @details This class is crucial for correctly parsing HTTP responses that use a `Content-Length`
 * header on a persistent (keep-alive) connection. It prevents a JSON parser or other reader
 * from accidentally reading past the end of the response body and into the start of the next
 * HTTP response on the same connection, which would lead to parsing errors and connection
 * desynchronization. It works by wrapping an existing `Stream` object (like `WiFiClient`)
 * and only allowing a specified number of bytes to be read.
 */
class SizedStream : public Stream {
private:
    Stream& _stream;        /**< A reference to the underlying stream (e.g., WiFiClient). */
    size_t _remaining_len;  /**< The number of bytes still allowed to be read. */

public:
    /**
     * @brief Constructs a new SizedStream object.
     * @param s The underlying stream to wrap.
     * @param len The maximum number of bytes to allow reading from the stream.
     */
    SizedStream(Stream& s, size_t len)
        : _stream(s), _remaining_len(len) {}

    /**
     * @brief Returns the number of bytes available to be read.
     * @return The smaller of the underlying stream's available bytes or the remaining
     *         length this wrapper is allowed to read.
     */
    virtual int available() {
        return min((size_t)_stream.available(), _remaining_len);
    }

    /**
     * @brief Reads a single byte from the stream.
     * @details If a byte is successfully read, the internal remaining length counter is
     * decremented. The read will fail (return -1) if the underlying stream has no data
     * or if the specified total length has already been read.
     * @return The byte read as an `int`, or -1 if no data is available or the limit is reached.
     */
    virtual int read() {
        if (_remaining_len == 0) {
            return -1; // Stop if we've read the entire specified length.
        }

        int val = _stream.read();
        if (val != -1) {
            _remaining_len--; // Successfully read a byte, so decrement our remaining length.
        } else {
            // The underlying stream ended before we read the full length.
            _remaining_len = 0; // Set remaining length to 0 to signal we are done.
        }
        return val;
    }

    /**
     * @brief Peeks at the next byte in the stream without consuming it.
     * @return The next byte as an `int`, or -1 if no data is available or the limit is reached.
     */
    virtual int peek() {
        if (_remaining_len == 0) {
            return -1;
        }
        // Peek doesn't consume the byte, so we don't decrement _remaining_len.
        return _stream.peek();
    }

    /**
     * @brief Flushes the underlying stream.
     */
    virtual void flush() {
        _stream.flush();
    }

    /**
     * @brief Not implemented for this read-only use case.
     * @return Always returns 0.
     */
    virtual size_t write(uint8_t data) {
        return 0;
    }
};
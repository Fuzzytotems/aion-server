#pragma once

#include "aion/commons/utils/ByteBuffer.h"

namespace aion::chatserver::common::netty {

/**
 * Netty's org.jboss.netty.buffer.ChannelBuffer as the client side of the chat server uses it, mapped onto commons' ByteBuffer: the buffers are
 * little endian heap buffers (Java: HeapChannelBufferFactory.getInstance(ByteOrder.LITTLE_ENDIAN) for received data,
 * ChannelBuffers.buffer(ByteOrder.LITTLE_ENDIAN, 2 * 8192) for sent packets).
 * <ul>
 * <li>Received frame: readerIndex = position, writerIndex = limit. Reads past the writer index throw (Netty: IndexOutOfBoundsException, here
 * utils::BufferUnderflowException) without moving the reader index.</li>
 * <li>Packet being written: writerIndex = position, the fixed capacity is the capacity. Writes past it throw (Netty: IndexOutOfBoundsException,
 * here utils::BufferOverflowException). LoginPacketEncoder flips the buffer, so [position, limit) are the readable bytes afterwards.</li>
 * </ul>
 */
using ChannelBuffer = commons::utils::ByteBuffer;

} // namespace aion::chatserver::common::netty

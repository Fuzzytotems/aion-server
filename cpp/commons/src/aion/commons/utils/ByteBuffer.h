#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string>

#include "aion/commons/utils/Exception.h"

namespace aion::commons::utils {

/** Java: java.nio.BufferUnderflowException */
class BufferUnderflowException : public Exception {
public:
	BufferUnderflowException() : Exception("Buffer underflow", std::stacktrace::current(1)) {}
};

/** Java: java.nio.BufferOverflowException */
class BufferOverflowException : public Exception {
public:
	BufferOverflowException() : Exception("Buffer overflow", std::stacktrace::current(1)) {}
};

enum class ByteOrder { LITTLE_ENDIAN_ORDER, BIG_ENDIAN_ORDER };

/**
 * A byte buffer with the semantics of java.nio.ByteBuffer (heap buffers only): capacity, limit, position, flip/compact/slice and relative or
 * absolute typed get/put.
 * <p>
 * Deviation: the default byte order of allocate() and wrap() is little endian (Java: big endian), since every Aion protocol uses it, and slice()
 * keeps the byte order of the buffer (Java: always big endian). Code that relies on Java's default, such as readers of big endian data files
 * (e.g. the game server's GeoWorldLoader), must set <tt>order(ByteOrder::BIG_ENDIAN_ORDER)</tt> after wrap() or allocate(); its slices then
 * stay big endian like in Java.
 * <p>
 * A ByteBuffer is a cheap handle: copies (and slices) share the underlying storage, like Java's duplicate()/slice(). Storage is kept alive by
 * reference counting when the buffer was created via allocate(); buffers created via wrap() do not own their memory.
 * <p>
 * Relative gets throw BufferUnderflowException and relative puts throw BufferOverflowException without modifying the position, like Java.
 */
class ByteBuffer {
public:
	/** Creates an empty buffer with zero capacity. */
	ByteBuffer() = default;

	/** Java: ByteBuffer.allocate(capacity). The contents are zero-initialized. */
	static ByteBuffer allocate(int32_t capacity);

	/** Java: ByteBuffer.wrap(array). The buffer does not own the memory, which must outlive it. */
	static ByteBuffer wrap(std::span<uint8_t> memory);

	int32_t capacity() const noexcept { return cap; }
	int32_t position() const noexcept { return pos; }
	int32_t limit() const noexcept { return lim; }
	int32_t remaining() const noexcept { return lim - pos; }
	bool hasRemaining() const noexcept { return pos < lim; }
	ByteOrder order() const noexcept { return byteOrder; }

	ByteBuffer& position(int32_t newPosition);
	ByteBuffer& limit(int32_t newLimit);
	ByteBuffer& order(ByteOrder newOrder) noexcept {
		byteOrder = newOrder;
		return *this;
	}

	/** Sets limit = position, position = 0. */
	ByteBuffer& flip() noexcept;
	/** Sets position = 0, limit = capacity. Does not erase the contents. */
	ByteBuffer& clear() noexcept;
	/** Sets position = 0, keeps the limit. */
	ByteBuffer& rewind() noexcept;
	/** Moves the remaining bytes to the beginning, then position = remaining, limit = capacity. */
	ByteBuffer& compact() noexcept;
	ByteBuffer& mark() noexcept {
		markPos = pos;
		return *this;
	}
	ByteBuffer& reset();

	/**
	 * Java: slice(). A new buffer over [position, limit) sharing this storage, with position 0 and capacity = limit = remaining().
	 * Deviation: keeps the byte order (see class comment).
	 */
	ByteBuffer slice() const;
	/** Java: slice(index, length). Deviation: keeps the byte order (see class comment). */
	ByteBuffer slice(int32_t index, int32_t length) const;

	/** Pointer to index 0 of this buffer (Java: array() + arrayOffset()). */
	uint8_t* data() noexcept { return base; }
	const uint8_t* data() const noexcept { return base; }
	/** The whole buffer from index 0 to capacity. */
	std::span<uint8_t> span() noexcept { return {base, static_cast<size_t>(cap)}; }
	std::span<const uint8_t> span() const noexcept { return {base, static_cast<size_t>(cap)}; }
	/** The bytes between position and limit. */
	std::span<uint8_t> remainingSpan() noexcept { return {base + pos, static_cast<size_t>(lim - pos)}; }
	std::span<const uint8_t> remainingSpan() const noexcept { return {base + pos, static_cast<size_t>(lim - pos)}; }

	// relative gets (advance the position)
	int8_t get() { return static_cast<int8_t>(nextGet<uint8_t>()); }
	int16_t getShort() { return static_cast<int16_t>(nextGet<uint16_t>()); }
	char16_t getChar() { return static_cast<char16_t>(nextGet<uint16_t>()); }
	int32_t getInt() { return static_cast<int32_t>(nextGet<uint32_t>()); }
	int64_t getLong() { return static_cast<int64_t>(nextGet<uint64_t>()); }
	float getFloat() { return std::bit_cast<float>(nextGet<uint32_t>()); }
	double getDouble() { return std::bit_cast<double>(nextGet<uint64_t>()); }
	/** Java: get(byte[] dst). Reads dst.size() bytes. */
	ByteBuffer& get(std::span<uint8_t> dst);

	// absolute gets (do not change the position)
	int8_t get(int32_t index) const { return static_cast<int8_t>(getAt<uint8_t>(index)); }
	int16_t getShort(int32_t index) const { return static_cast<int16_t>(getAt<uint16_t>(index)); }
	char16_t getChar(int32_t index) const { return static_cast<char16_t>(getAt<uint16_t>(index)); }
	int32_t getInt(int32_t index) const { return static_cast<int32_t>(getAt<uint32_t>(index)); }
	int64_t getLong(int32_t index) const { return static_cast<int64_t>(getAt<uint64_t>(index)); }
	float getFloat(int32_t index) const { return std::bit_cast<float>(getAt<uint32_t>(index)); }
	double getDouble(int32_t index) const { return std::bit_cast<double>(getAt<uint64_t>(index)); }

	// relative puts (advance the position)
	ByteBuffer& put(int8_t value) { return nextPut(static_cast<uint8_t>(value)); }
	ByteBuffer& put(uint8_t value) { return nextPut(value); }
	ByteBuffer& putShort(int16_t value) { return nextPut(static_cast<uint16_t>(value)); }
	ByteBuffer& putChar(char16_t value) { return nextPut(static_cast<uint16_t>(value)); }
	ByteBuffer& putInt(int32_t value) { return nextPut(static_cast<uint32_t>(value)); }
	ByteBuffer& putLong(int64_t value) { return nextPut(static_cast<uint64_t>(value)); }
	ByteBuffer& putFloat(float value) { return nextPut(std::bit_cast<uint32_t>(value)); }
	ByteBuffer& putDouble(double value) { return nextPut(std::bit_cast<uint64_t>(value)); }
	/** Java: put(byte[] src) */
	ByteBuffer& put(std::span<const uint8_t> src);
	/** Java: put(ByteBuffer src). Transfers src.remaining() bytes and advances both positions. */
	ByteBuffer& put(ByteBuffer& src);

	// absolute puts (do not change the position)
	ByteBuffer& put(int32_t index, int8_t value) { return putAt(index, static_cast<uint8_t>(value)); }
	ByteBuffer& putShort(int32_t index, int16_t value) { return putAt(index, static_cast<uint16_t>(value)); }
	ByteBuffer& putChar(int32_t index, char16_t value) { return putAt(index, static_cast<uint16_t>(value)); }
	ByteBuffer& putInt(int32_t index, int32_t value) { return putAt(index, static_cast<uint32_t>(value)); }
	ByteBuffer& putLong(int32_t index, int64_t value) { return putAt(index, static_cast<uint64_t>(value)); }
	ByteBuffer& putFloat(int32_t index, float value) { return putAt(index, std::bit_cast<uint32_t>(value)); }
	ByteBuffer& putDouble(int32_t index, double value) { return putAt(index, std::bit_cast<uint64_t>(value)); }

	/** Java: ByteBuffer.toString() - e.g. "ByteBuffer[pos=0 lim=10 cap=10]" */
	std::string toString() const;

private:
	ByteBuffer(std::shared_ptr<uint8_t[]> storage, uint8_t* base, int32_t capacity, ByteOrder order);

	template <typename T>
	T toOrder(T value) const noexcept {
		bool nativeLittle = std::endian::native == std::endian::little;
		bool wantLittle = byteOrder == ByteOrder::LITTLE_ENDIAN_ORDER;
		return (nativeLittle == wantLittle || sizeof(T) == 1) ? value : std::byteswap(value);
	}

	template <typename T>
	T nextGet() {
		if (lim - pos < static_cast<int32_t>(sizeof(T)))
			throw BufferUnderflowException();
		T value;
		std::memcpy(&value, base + pos, sizeof(T));
		pos += sizeof(T);
		return toOrder(value);
	}

	template <typename T>
	T getAt(int32_t index) const {
		checkIndex(index, sizeof(T));
		T value;
		std::memcpy(&value, base + index, sizeof(T));
		return toOrder(value);
	}

	template <typename T>
	ByteBuffer& nextPut(T value) {
		if (lim - pos < static_cast<int32_t>(sizeof(T)))
			throw BufferOverflowException();
		value = toOrder(value);
		std::memcpy(base + pos, &value, sizeof(T));
		pos += sizeof(T);
		return *this;
	}

	template <typename T>
	ByteBuffer& putAt(int32_t index, T value) {
		checkIndex(index, sizeof(T));
		value = toOrder(value);
		std::memcpy(base + index, &value, sizeof(T));
		return *this;
	}

	void checkIndex(int32_t index, size_t size) const;

	std::shared_ptr<uint8_t[]> storage; // null for wrapped (non-owning) buffers
	uint8_t* base = nullptr;
	int32_t cap = 0;
	int32_t lim = 0;
	int32_t pos = 0;
	int32_t markPos = -1;
	ByteOrder byteOrder = ByteOrder::LITTLE_ENDIAN_ORDER;
};

} // namespace aion::commons::utils

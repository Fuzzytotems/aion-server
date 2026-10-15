#include "aion/commons/utils/ByteBuffer.h"

#include <fmt/format.h>

namespace aion::commons::utils {

ByteBuffer::ByteBuffer(std::shared_ptr<uint8_t[]> storage, uint8_t* base, int32_t capacity, ByteOrder order)
	: storage(std::move(storage)), base(base), cap(capacity), lim(capacity), byteOrder(order) {
}

ByteBuffer ByteBuffer::allocate(int32_t capacity) {
	if (capacity < 0)
		throw IllegalArgumentException(fmt::format("capacity < 0: ({} < 0)", capacity));
	auto storage = std::make_shared<uint8_t[]>(static_cast<size_t>(capacity)); // value-initialized (zeroed)
	uint8_t* base = storage.get();
	return ByteBuffer(std::move(storage), base, capacity, ByteOrder::LITTLE_ENDIAN_ORDER);
}

ByteBuffer ByteBuffer::wrap(std::span<uint8_t> memory) {
	return ByteBuffer(nullptr, memory.data(), static_cast<int32_t>(memory.size()), ByteOrder::LITTLE_ENDIAN_ORDER);
}

ByteBuffer& ByteBuffer::position(int32_t newPosition) {
	if (newPosition > lim || newPosition < 0)
		throw IllegalArgumentException(fmt::format("newPosition > limit: ({} > {})", newPosition, lim));
	if (markPos > newPosition)
		markPos = -1;
	pos = newPosition;
	return *this;
}

ByteBuffer& ByteBuffer::limit(int32_t newLimit) {
	if (newLimit > cap || newLimit < 0)
		throw IllegalArgumentException(fmt::format("newLimit > capacity: ({} > {})", newLimit, cap));
	lim = newLimit;
	if (pos > newLimit)
		pos = newLimit;
	if (markPos > newLimit)
		markPos = -1;
	return *this;
}

ByteBuffer& ByteBuffer::flip() noexcept {
	lim = pos;
	pos = 0;
	markPos = -1;
	return *this;
}

ByteBuffer& ByteBuffer::clear() noexcept {
	pos = 0;
	lim = cap;
	markPos = -1;
	return *this;
}

ByteBuffer& ByteBuffer::rewind() noexcept {
	pos = 0;
	markPos = -1;
	return *this;
}

ByteBuffer& ByteBuffer::compact() noexcept {
	int32_t rem = remaining();
	if (rem > 0 && pos > 0)
		std::memmove(base, base + pos, static_cast<size_t>(rem));
	pos = rem;
	lim = cap;
	markPos = -1;
	return *this;
}

ByteBuffer& ByteBuffer::reset() {
	if (markPos < 0)
		throw IllegalStateException("InvalidMarkException");
	pos = markPos;
	return *this;
}

ByteBuffer ByteBuffer::slice() const {
	return ByteBuffer(storage, base + pos, remaining(), byteOrder);
}

ByteBuffer ByteBuffer::slice(int32_t index, int32_t length) const {
	if (index < 0 || length < 0 || index > lim - length)
		throw IndexOutOfBoundsException(fmt::format("slice({}, {}) out of bounds for limit {}", index, length, lim));
	return ByteBuffer(storage, base + index, length, byteOrder);
}

ByteBuffer& ByteBuffer::get(std::span<uint8_t> dst) {
	if (static_cast<size_t>(remaining()) < dst.size())
		throw BufferUnderflowException();
	std::memcpy(dst.data(), base + pos, dst.size());
	pos += static_cast<int32_t>(dst.size());
	return *this;
}

ByteBuffer& ByteBuffer::put(std::span<const uint8_t> src) {
	if (static_cast<size_t>(remaining()) < src.size())
		throw BufferOverflowException();
	std::memmove(base + pos, src.data(), src.size());
	pos += static_cast<int32_t>(src.size());
	return *this;
}

ByteBuffer& ByteBuffer::put(ByteBuffer& src) {
	if (&src == this)
		throw IllegalArgumentException("The source buffer is this buffer");
	int32_t n = src.remaining();
	if (n > remaining())
		throw BufferOverflowException();
	std::memmove(base + pos, src.base + src.pos, static_cast<size_t>(n));
	pos += n;
	src.pos += n;
	return *this;
}

void ByteBuffer::checkIndex(int32_t index, size_t size) const {
	if (index < 0 || static_cast<int64_t>(index) + static_cast<int64_t>(size) > lim)
		throw IndexOutOfBoundsException(fmt::format("Index {} (size {}) out of bounds for limit {}", index, size, lim));
}

std::string ByteBuffer::toString() const {
	return fmt::format("ByteBuffer[pos={} lim={} cap={}]", pos, lim, cap);
}

} // namespace aion::commons::utils

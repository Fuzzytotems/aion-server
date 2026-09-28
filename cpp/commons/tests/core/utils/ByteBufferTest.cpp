#include <gtest/gtest.h>

#include "aion/commons/utils/ByteBuffer.h"

using namespace aion::commons::utils;

TEST(ByteBufferTest, RelativePutGetLittleEndian) {
	ByteBuffer buf = ByteBuffer::allocate(32);
	buf.put(int8_t{-1}).putShort(0x1234).putInt(0x12345678).putLong(0x0102030405060708LL).putFloat(1.5f).putDouble(-2.25).putChar(u'Ä');
	EXPECT_EQ(buf.position(), 1 + 2 + 4 + 8 + 4 + 8 + 2);
	EXPECT_EQ(buf.data()[1], 0x34); // little endian
	EXPECT_EQ(buf.data()[2], 0x12);
	buf.flip();
	EXPECT_EQ(buf.get(), -1);
	EXPECT_EQ(buf.getShort(), 0x1234);
	EXPECT_EQ(buf.getInt(), 0x12345678);
	EXPECT_EQ(buf.getLong(), 0x0102030405060708LL);
	EXPECT_FLOAT_EQ(buf.getFloat(), 1.5f);
	EXPECT_DOUBLE_EQ(buf.getDouble(), -2.25);
	EXPECT_EQ(buf.getChar(), u'Ä');
	EXPECT_FALSE(buf.hasRemaining());
}

TEST(ByteBufferTest, BigEndianOrder) {
	ByteBuffer buf = ByteBuffer::allocate(4).order(ByteOrder::BIG_ENDIAN_ORDER);
	buf.putInt(0x0A0B0C0D);
	EXPECT_EQ(buf.data()[0], 0x0A);
	EXPECT_EQ(buf.getInt(0), 0x0A0B0C0D);
}

TEST(ByteBufferTest, UnderflowDoesNotMovePosition) {
	ByteBuffer buf = ByteBuffer::allocate(3);
	EXPECT_THROW(buf.getInt(), BufferUnderflowException);
	EXPECT_EQ(buf.position(), 0);
	EXPECT_THROW(buf.putInt(1), BufferOverflowException);
	EXPECT_EQ(buf.position(), 0);
	EXPECT_THROW(buf.getShort(2), IndexOutOfBoundsException);
}

TEST(ByteBufferTest, CompactAndSliceShareStorage) {
	ByteBuffer buf = ByteBuffer::allocate(8);
	for (int8_t i = 0; i < 8; i++)
		buf.put(i);
	buf.flip();
	buf.position(2);
	ByteBuffer slice = buf.slice();
	EXPECT_EQ(slice.capacity(), 6);
	EXPECT_EQ(slice.get(0), 2);
	slice.put(0, int8_t{42});
	EXPECT_EQ(buf.get(2), 42); // shared storage
	buf.compact();
	EXPECT_EQ(buf.position(), 6);
	EXPECT_EQ(buf.limit(), 8);
	EXPECT_EQ(buf.get(0), 42);
	EXPECT_EQ(buf.get(5), 7);
}

TEST(ByteBufferTest, SliceKeepsStorageAlive) {
	ByteBuffer slice;
	{
		ByteBuffer buf = ByteBuffer::allocate(4);
		buf.putInt(0, 77);
		slice = buf.slice();
	}
	EXPECT_EQ(slice.getInt(0), 77);
}

TEST(ByteBufferTest, LimitClampsPosition) {
	ByteBuffer buf = ByteBuffer::allocate(10);
	buf.position(8);
	buf.limit(5);
	EXPECT_EQ(buf.position(), 5);
	EXPECT_THROW(buf.position(6), IllegalArgumentException);
}

TEST(ByteBufferTest, DefaultOrderAndSlicesOfBigEndianBuffers) {
	EXPECT_EQ(ByteBuffer::allocate(1).order(), ByteOrder::LITTLE_ENDIAN_ORDER); // Deviation: Java's default is big endian
	uint8_t data[] = {0x00, 0x02, 0x3F, 0xC0, 0x00, 0x00};
	// GeoWorldLoader: geo.getShort() and geo.slice(pos, n).asFloatBuffer() read big endian data
	ByteBuffer geo = ByteBuffer::wrap(data).order(ByteOrder::BIG_ENDIAN_ORDER);
	EXPECT_EQ(geo.getShort(), 2);
	ByteBuffer vertices = geo.slice(2, 4);
	EXPECT_EQ(vertices.order(), ByteOrder::BIG_ENDIAN_ORDER);
	EXPECT_FLOAT_EQ(vertices.getFloat(), 1.5f);
}

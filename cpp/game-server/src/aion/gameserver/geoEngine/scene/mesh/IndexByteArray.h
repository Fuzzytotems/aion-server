#pragma once

#include <cstdint>
#include <span>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/geoEngine/scene/mesh/IndexArray.h"
#include "aion/gameserver/geoEngine/scene/mesh/fwd.h"

namespace aion::gameserver::geoEngine::scene::mesh {

/**
 * Triangle indices stored as unsigned bytes (Java: record IndexByteArray(byte[] buf)).
 * <p>
 * RefCounted (fieldmap K4), created with create(). The record's equals/hashCode compare the array reference (Java arrays have identity
 * equality), so equals compares `buf` by identity.
 */
class IndexByteArray : public runtime::RefCounted, public IndexArray {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<runtime::Array<int8_t>> buf;

protected:
	/** Java: IndexByteArray(byte[] buf) (canonical record constructor) */
	explicit IndexByteArray(runtime::Ref<runtime::Array<int8_t>> buf);
	~IndexByteArray() override;

public:
	void retain() const noexcept override { runtime::RefCounted::retain(); }
	void release() const noexcept override { runtime::RefCounted::release(); }

	/** Java: new IndexByteArray(byte[] buf) */
	static runtime::Ref<IndexByteArray> create(runtime::Ref<runtime::Array<int8_t>> buf);

	/** Java: new IndexByteArray(ByteBuffer buf) - copies the buffer's bytes */
	static runtime::Ref<IndexByteArray> create(std::span<const int8_t> buf);

	/** Java: byte[] buf() (record accessor) */
	runtime::Ptr<runtime::Array<int8_t>> getBuf() const { return buf; }

	int32_t get(int32_t i) override;

	int32_t size() override;

	void swap(int32_t i1, int32_t i2) override;

	bool equals(const IndexByteArray& obj) const { return buf.get() == obj.buf.get(); }

	int32_t hashCode() const;
};

} // namespace aion::gameserver::geoEngine::scene::mesh

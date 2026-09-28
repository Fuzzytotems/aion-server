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
 * Triangle indices stored as unsigned shorts (Java: record IndexShortArray(short[] buf)).
 * <p>
 * RefCounted (fieldmap K4), created with create(). The record's equals/hashCode compare the array reference (Java arrays have identity
 * equality), so equals compares `buf` by identity.
 */
class IndexShortArray : public runtime::RefCounted, public IndexArray {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<runtime::Array<int16_t>> buf;

protected:
	/** Java: IndexShortArray(short[] buf) (canonical record constructor) */
	explicit IndexShortArray(runtime::Ref<runtime::Array<int16_t>> buf);
	~IndexShortArray() override;

public:
	void retain() const noexcept override { runtime::RefCounted::retain(); }
	void release() const noexcept override { runtime::RefCounted::release(); }

	/** Java: new IndexShortArray(short[] buf) */
	static runtime::Ref<IndexShortArray> create(runtime::Ref<runtime::Array<int16_t>> buf);

	/** Java: new IndexShortArray(ShortBuffer buf) - copies the buffer's shorts */
	static runtime::Ref<IndexShortArray> create(std::span<const int16_t> buf);

	/** Java: short[] buf() (record accessor) */
	runtime::Ptr<runtime::Array<int16_t>> getBuf() const { return buf; }

	int32_t get(int32_t i) override;

	int32_t size() override;

	void swap(int32_t i1, int32_t i2) override;

	bool equals(const IndexShortArray& obj) const { return buf.get() == obj.buf.get(); }

	int32_t hashCode() const;
};

} // namespace aion::gameserver::geoEngine::scene::mesh

#pragma once

#include <cstdint>
#include <span>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/geoEngine/scene/mesh/fwd.h"

namespace aion::gameserver::geoEngine::scene::mesh {

/**
 * The triangle vertex indices of a Mesh: three indices per triangle, read as unsigned bytes or unsigned shorts.
 * <p>
 * Interface held by `Ref<IndexArray>` (Mesh.indices), so it declares the reference count operations (hub-headers.md §9.2); the implementing
 * records forward them to RefCounted. Java's `from(Buffer)` switch over ByteBuffer and ShortBuffer is the two `from` overloads (a buffer of
 * another type cannot be passed, so the IllegalArgumentException branch has no counterpart).
 */
class IndexArray {
public:
	/** C++ only: Ref<IndexArray> retains the implementing object (hub-headers.md §9.2). */
	virtual void retain() const noexcept = 0;
	virtual void release() const noexcept = 0;

	virtual int32_t get(int32_t i) = 0;

	virtual int32_t size() = 0;

	/** Swaps the three indices of the triangles i1 and i2. */
	virtual void swap(int32_t i1, int32_t i2) = 0;

	/** Java: IndexArray.from(ByteBuffer) - copies the buffer's bytes */
	static runtime::Ref<IndexArray> from(std::span<const int8_t> buffer);

	/** Java: IndexArray.from(ShortBuffer) - copies the buffer's shorts */
	static runtime::Ref<IndexArray> from(std::span<const int16_t> buffer);

	virtual ~IndexArray() = default;

protected:
	IndexArray() = default;
	IndexArray(const IndexArray&) = default;
	IndexArray& operator=(const IndexArray&) = default;
};

} // namespace aion::gameserver::geoEngine::scene::mesh

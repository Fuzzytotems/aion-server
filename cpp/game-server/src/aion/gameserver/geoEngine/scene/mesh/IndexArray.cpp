#include "aion/gameserver/geoEngine/scene/mesh/IndexArray.h"

#include "aion/gameserver/geoEngine/scene/mesh/IndexByteArray.h"
#include "aion/gameserver/geoEngine/scene/mesh/IndexShortArray.h"

namespace aion::gameserver::geoEngine::scene::mesh {

runtime::Ref<IndexArray> IndexArray::from(std::span<const int8_t> buffer) {
	return IndexByteArray::create(buffer);
}

runtime::Ref<IndexArray> IndexArray::from(std::span<const int16_t> buffer) {
	return IndexShortArray::create(buffer);
}

} // namespace aion::gameserver::geoEngine::scene::mesh

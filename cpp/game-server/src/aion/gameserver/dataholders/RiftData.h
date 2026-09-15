#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/RiftData.xml.h"
#include "aion/gameserver/dataholders/detail/LinkedMap.h"
#include "aion/gameserver/model/rift/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.RiftData.
 * <p>
 * C++: the rift locations are RefCounted run-time objects (P5-12b) created by the hook and owned by the published holder; Java's LinkedHashMap
 * is a detail::LinkedMap (read-only after publish).
 *
 * @author Source
 */
class RiftData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/RiftData.xml.inc"
private:
	detail::LinkedMap<int32_t, runtime::Ref<model::rift::RiftLocation>> rift;

public:
	/** C++ only: out of line, where the RefCounted element types are complete */
	RiftData();
	~RiftData();

	int32_t size() const;

	const detail::LinkedMap<int32_t, runtime::Ref<model::rift::RiftLocation>>& getRiftLocations() const;
};

} // namespace aion::gameserver::dataholders

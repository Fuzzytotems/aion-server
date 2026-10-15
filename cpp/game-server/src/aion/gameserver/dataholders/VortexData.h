#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/VortexData.xml.h"
#include "aion/gameserver/dataholders/detail/LinkedMap.h"
#include "aion/gameserver/model/vortex/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.VortexData.
 * <p>
 * C++: the vortex locations are RefCounted run-time objects (P5-12b) created by the hook and owned by the published holder; Java's
 * LinkedHashMap is a detail::LinkedMap (read-only after publish).
 *
 * @author Source
 */
class VortexData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/VortexData.xml.inc"
private:
	detail::LinkedMap<int32_t, runtime::Ref<model::vortex::VortexLocation>> vortex;

public:
	/** C++ only: out of line, where the RefCounted element types are complete */
	VortexData();
	~VortexData();

	int32_t size() const;

	/** @return the first location (insertion order) of the invasion world, nullptr (Java null) if there is none */
	runtime::Ptr<model::vortex::VortexLocation> getVortexLocation(int32_t invasionWorldId) const;

	const detail::LinkedMap<int32_t, runtime::Ref<model::vortex::VortexLocation>>& getVortexLocations() const;
};

} // namespace aion::gameserver::dataholders

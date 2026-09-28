#include "aion/gameserver/dataholders/CustomDrop.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dataholders {

const model::drop::NpcDrop* CustomDrop::getNpcDrop(int32_t npcId) const {
	auto it = dropById.find(npcId);
	return it != dropById.end() ? it->second : nullptr;
}

void CustomDrop::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::drop::NpcDrop& drop : npcDrop) {
		if (!dropById.try_emplace(drop.getNpcId(), &drop).second)
			commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.CustomDrop")
			  .warn("Tried to set custom drop for npc " + std::to_string(drop.getNpcId()) + " twice!");
	}
	// Java: npcDrop = null (the C++ index points into the storage, which stays)
}

int32_t CustomDrop::size() const {
	return static_cast<int32_t>(dropById.size());
}

} // namespace aion::gameserver::dataholders

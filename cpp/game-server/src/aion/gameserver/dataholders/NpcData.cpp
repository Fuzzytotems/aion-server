#include "aion/gameserver/dataholders/NpcData.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

void NpcData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	AION_UNPORTED();
}

const model::templates::npc::NpcTemplate* NpcData::getNpcTemplate(int32_t id) const {
	auto it = npcData.find(id);
	return it != npcData.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders

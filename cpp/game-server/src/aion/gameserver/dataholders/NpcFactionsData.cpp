#include "aion/gameserver/dataholders/NpcFactionsData.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

void NpcFactionsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	AION_UNPORTED();
}

const model::templates::factions::NpcFactionTemplate* NpcFactionsData::getNpcFactionById(int32_t id) const {
	AION_UNPORTED();
}

const model::templates::factions::NpcFactionTemplate* NpcFactionsData::getNpcFactionByNpcId(int32_t id) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dataholders

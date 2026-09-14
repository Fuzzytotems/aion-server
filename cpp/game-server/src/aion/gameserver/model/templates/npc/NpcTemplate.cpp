#include "aion/gameserver/model/templates/npc/NpcTemplate.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/BoundRadius.h"

namespace aion::gameserver::model::templates::npc {

void NpcTemplate::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	AION_UNPORTED();
}

const BoundRadius* NpcTemplate::getBoundRadius() const {
	// Java comment: all npcs should have BR in xml
	return boundRadius != nullptr ? boundRadius.get() : VisibleObjectTemplate::getBoundRadius();
}

void NpcTemplate::setXmlUid(std::string_view /*uid*/) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::npc

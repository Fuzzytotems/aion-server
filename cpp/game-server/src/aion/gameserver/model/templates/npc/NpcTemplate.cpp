#include "aion/gameserver/model/templates/npc/NpcTemplate.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::templates::npc {

void NpcTemplate::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	AION_UNPORTED();
}

void NpcTemplate::setXmlUid(std::string_view /*uid*/) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::npc

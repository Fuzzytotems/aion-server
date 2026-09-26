#include "aion/gameserver/model/templates/npc/NpcTemplate.h"

#include <algorithm>
#include <string>

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/model/templates/BoundRadius.h"

namespace aion::gameserver::model::templates::npc {

void NpcTemplate::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	if (level > 1 && ai != "noaction" && getAbyssNpcType() == AbyssNpcType::TELEPORTER) // TODO: reparse npc_template
		ai = "siege_teleporter";
	// Java: ai = ai.intern() (C++ strings are values)
}

std::string NpcTemplate::toString() const {
	return "Npc Template id: " + std::to_string(npcId) + " name: " + name;
}

void NpcTemplate::setXmlUid(std::string_view uid) {
	// This method is used only by the binder: the ID must be a string.
	npcId = commons::utils::parseInt(uid);
}

const BoundRadius* NpcTemplate::getBoundRadius() const {
	// Java comment: all npcs should have BR in xml
	return boundRadius != nullptr ? boundRadius.get() : VisibleObjectTemplate::getBoundRadius();
}

const std::vector<int32_t>* NpcTemplate::getFuncDialogIds() const {
	if (talkInfo == nullptr || !talkInfo->getFuncDialogIds().has_value())
		return nullptr;
	return &*talkInfo->getFuncDialogIds();
}

bool NpcTemplate::supportsAction(int32_t dialogActionId) const {
	const std::vector<int32_t>* dialogIds = getFuncDialogIds();
	return dialogIds != nullptr && std::ranges::find(*dialogIds, dialogActionId) != dialogIds->end();
}

const MassiveLoot& NpcTemplate::requireMassiveLoot() const {
	if (massiveLoot == nullptr)
		throw runtime::NullPointerException("NpcTemplate " + std::to_string(npcId) + " has no massive_loot");
	return *massiveLoot;
}

int32_t NpcTemplate::getMassiveLootCount() const {
	return requireMassiveLoot().getMLootCount();
}

int32_t NpcTemplate::getMassiveLootItem() const {
	return requireMassiveLoot().getMLootItem();
}

int32_t NpcTemplate::getMassiveLootMinLevel() const {
	return requireMassiveLoot().getMLootMinLevel();
}

int32_t NpcTemplate::getMassiveLootMaxLevel() const {
	return requireMassiveLoot().getMLootMaxLevel();
}

} // namespace aion::gameserver::model::templates::npc

#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/autogroup/AutoGroup.xml.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::autogroup {

/** Java com.aionemu.gameserver.model.autogroup.AutoGroup. @author MrPoke */
class AutoGroup : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/autogroup/AutoGroup.xml.inc"
public:
	int32_t getL10nId() const override { return nameId; }

	/** @return the portal npc ids, empty if the attribute is absent (Java: npcIds == null ? Collections.emptyList() : npcIds) */
	const std::vector<int32_t>& getNpcIds() const {
		static const std::vector<int32_t> empty;
		return npcIds ? *npcIds : empty;
	}

	bool isRecruitableInstance() const { return (id >= 302 && id < 400) || instanceId == 300600000 || instanceId == 300220000; }
};

} // namespace aion::gameserver::model::autogroup

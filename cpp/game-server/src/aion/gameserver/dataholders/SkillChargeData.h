#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "aion/gameserver/dataholders/SkillChargeData.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.SkillChargeData.
 * <p>
 * C++: the index points into the bound `chargeSkills` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author Rolandas
 */
class SkillChargeData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/SkillChargeData.xml.inc"
private:
	std::unordered_map<int32_t, const skillengine::model::ChargeSkillEntry*> skillChargeData;
	std::unordered_set<int32_t> skillIds;

public:
	/** @return the charge skill entry, nullptr (Java null) if there is none */
	const skillengine::model::ChargeSkillEntry* getChargedSkillEntry(int32_t chargeId) const;

	bool isChargeSkill(const skillengine::model::SkillTemplate& skillTemplate) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders

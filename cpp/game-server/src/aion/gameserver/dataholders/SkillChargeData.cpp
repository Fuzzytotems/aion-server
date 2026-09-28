#include "aion/gameserver/dataholders/SkillChargeData.h"

#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::dataholders {

void SkillChargeData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const skillengine::model::ChargeSkillEntry& chargeSkill : chargeSkills) {
		skillChargeData.insert_or_assign(chargeSkill.getId(), &chargeSkill);
		for (const skillengine::model::ChargedSkill& s : chargeSkill.getSkills())
			skillIds.insert(s.getId());
	}
	// Java: chargeSkills = null (the C++ index points into the storage, which stays)
}

const skillengine::model::ChargeSkillEntry* SkillChargeData::getChargedSkillEntry(int32_t chargeId) const {
	auto it = skillChargeData.find(chargeId);
	return it != skillChargeData.end() ? it->second : nullptr;
}

bool SkillChargeData::isChargeSkill(const skillengine::model::SkillTemplate& skillTemplate) const {
	return skillIds.contains(skillTemplate.getSkillId());
}

int32_t SkillChargeData::size() const {
	return static_cast<int32_t>(skillChargeData.size());
}

} // namespace aion::gameserver::dataholders

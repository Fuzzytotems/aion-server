#include "aion/gameserver/dataholders/PetSkillData.h"

#include <algorithm>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

void PetSkillData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::petskill::PetSkillTemplate& petSkill : petSkills) {
		petSkillData[petSkill.getOrderSkill()].insert_or_assign(petSkill.getPetId(), petSkill.getSkillId());
		petSkillsMap[petSkill.getPetId()].push_back(petSkill.getSkillId());
	}
	// Java: petSkills = null (the C++ maps copy the values)
}

int32_t PetSkillData::size() const {
	return static_cast<int32_t>(petSkillData.size());
}

bool PetSkillData::isPetOrderSkill(int32_t orderSkill) const {
	return petSkillData.contains(orderSkill);
}

int32_t PetSkillData::getPetOrderSkill(int32_t orderSkill, int32_t petNpcId) const {
	auto pets = petSkillData.find(orderSkill);
	if (pets == petSkillData.end())
		throw runtime::NullPointerException("Cannot invoke \"java.util.Map.get(Object)\": no pet order skill " + std::to_string(orderSkill));
	auto skill = pets->second.find(petNpcId);
	if (skill == pets->second.end())
		throw runtime::NullPointerException("Cannot invoke \"java.lang.Integer.intValue()\": no order skill " + std::to_string(orderSkill) + " for pet " +
		                                    std::to_string(petNpcId));
	return skill->second;
}

bool PetSkillData::petHasSkill(int32_t petNpcId, int32_t skillId) const {
	auto skills = petSkillsMap.find(petNpcId);
	if (skills == petSkillsMap.end())
		throw runtime::NullPointerException("Cannot invoke \"java.util.List.contains(Object)\": no skills for pet " + std::to_string(petNpcId));
	return std::find(skills->second.begin(), skills->second.end(), skillId) != skills->second.end();
}

} // namespace aion::gameserver::dataholders

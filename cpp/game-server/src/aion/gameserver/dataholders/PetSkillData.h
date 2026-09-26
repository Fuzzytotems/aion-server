#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/PetSkillData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.PetSkillData. @author ATracer */
class PetSkillData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PetSkillData.xml.inc"
private:
	std::unordered_map<int32_t, std::unordered_map<int32_t, int32_t>> petSkillData;
	std::unordered_map<int32_t, std::vector<int32_t>> petSkillsMap;

public:
	int32_t size() const;

	bool isPetOrderSkill(int32_t orderSkill) const;

	/** @throws NullPointerException if the order skill or the pet has no entry (Java unboxes a null Integer) */
	int32_t getPetOrderSkill(int32_t orderSkill, int32_t petNpcId) const;

	/** @throws NullPointerException if the pet has no skills (Java dereferences the null list) */
	bool petHasSkill(int32_t petNpcId, int32_t skillId) const;
};

} // namespace aion::gameserver::dataholders

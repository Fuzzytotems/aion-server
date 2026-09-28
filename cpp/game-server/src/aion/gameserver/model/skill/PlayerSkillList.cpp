#include "aion/gameserver/model/skill/PlayerSkillList.h"

#include <limits>

#include "aion/gameserver/configs/main/CraftConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/skillengine/model/SkillLearnTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::skill {

using gameobjects::Persistable_PersistentState;
using runtime::Ptr;
using runtime::Ref;

PlayerSkillList::PlayerSkillList() = default;

PlayerSkillList::PlayerSkillList(const std::vector<runtime::Ptr<PlayerSkillEntry>>& playerSkills) {
	for (const runtime::Ptr<PlayerSkillEntry>& entry : playerSkills)
		skills.put(entry->getSkillId(), runtime::Ref<PlayerSkillEntry>(entry));
}

PlayerSkillList::~PlayerSkillList() = default;

runtime::Ref<PlayerSkillList> PlayerSkillList::create() {
	return runtime::makeRef<PlayerSkillList>();
}

runtime::Ref<PlayerSkillList> PlayerSkillList::create(const std::vector<runtime::Ptr<PlayerSkillEntry>>& playerSkills) {
	return runtime::makeRef<PlayerSkillList>(playerSkills);
}

std::vector<runtime::Ptr<PlayerSkillEntry>> PlayerSkillList::getAllSkills() {
	std::vector<Ptr<PlayerSkillEntry>> allSkills;
	for (Ptr<PlayerSkillEntry> entry : skills.values())
		allSkills.push_back(entry);
	return allSkills;
}

std::vector<runtime::Ptr<PlayerSkillEntry>> PlayerSkillList::getDeletedSkills() {
	SYNCHRONIZED(deletedSkills) {
		return deletedSkills.snapshot();
	}
}

runtime::Ptr<PlayerSkillEntry> PlayerSkillList::getSkillEntry(int32_t skillId) {
	return skills.get(skillId);
}

bool PlayerSkillList::addSkill(gameobjects::Creature& player, int32_t skillId, int32_t skillLevel) {
	return addSkill(*runtime::cast<gameobjects::player::Player>(player), skillId, skillLevel, false);
}

bool PlayerSkillList::addTemporarySkill(gameobjects::player::Player& player, int32_t skillId, int32_t skillLevel) {
	return addSkill(player, skillId, skillLevel, true);
}

bool PlayerSkillList::addSkill(gameobjects::player::Player& player, int32_t skillId, int32_t skillLevel, bool isTemporary) {
	bool isNew = true;
	SYNCHRONIZED(*this) {
		Ptr<PlayerSkillEntry> existingSkill = skills.get(skillId);
		if (existingSkill) {
			if (skillLevel <= existingSkill->getSkillLevel())
				return false;
			existingSkill->setSkillLvl(skillLevel);
			isNew = false;
		} else {
			skills.put(skillId, PlayerSkillEntry::create(player, skillId, skillLevel,
									isTemporary ? Persistable_PersistentState::NOACTION : Persistable_PersistentState::NEW));
			std::vector<const skillengine::model::SkillLearnTemplate*> learnTemplates =
				dataholders::DataManager::SKILL_TREE_DATA->getSkillsForSkill(skillId, player.getPlayerClass(), player.getRace(), player.getLevel());
			for (const skillengine::model::SkillLearnTemplate* learnTemplate : learnTemplates) {
				if (learnTemplate->getLearnSkill() && skills.get(*learnTemplate->getLearnSkill())) {
					isNew = false;
					break;
				}
			}
		}
		services::SkillLearnService::onLearnSkill(player, skillId, skillLevel, isNew);
		return true;
	}
}

bool PlayerSkillList::addSkillXp(gameobjects::player::Player& player, int32_t skillId, int32_t xpReward, int32_t objSkillLvl) {
	SYNCHRONIZED(*this) {
		Ptr<PlayerSkillEntry> skill = getSkillEntry(skillId);
		int32_t skillLvl = skill->getSkillLevel(); // Java: NullPointerException for a skill the list does not hold
		if (static_cast<int32_t>(static_cast<uint32_t>(skillLvl) - static_cast<uint32_t>(objSkillLvl)) > 40)
			return false;

		switch (skillId) {
			case 30001:
				if (skillLvl == 49)
					return false; // human gathering is capped at 49 points
				[[fallthrough]];
			case 30002:
			case 30003:
				if (skillLvl == 449 || (skillLvl >= 499 && configs::main::CraftConfig::DISABLE_AETHER_AND_ESSENCE_TAPPING_CAP.load()))
					break; // break here to enable gather exp on master max lvl
				[[fallthrough]];
			case 40001:
			case 40002:
			case 40003:
			case 40004:
			case 40007:
			case 40008:
			case 40010:
				switch (skillLvl) {
					case 99:
					case 199:
					case 299:
					case 399:
					case 449:
					case 499:
					case 549:
						utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CRAFT_INFO_MAXPOINT_UP());
						return false; // disable exp gain to force mastering upgrade via npc
				}
		}

		// Java: (int) (0.23 * (skillLvl + 17.2) * (skillLvl + 17.2)) - double arithmetic and the saturating (int) cast
		const double base = static_cast<double>(skillLvl) + 17.2;
		const double product = 0.23 * base * base;
		int32_t requiredExp = product >= 2147483647.0 ? std::numeric_limits<int32_t>::max()
							  : product <= -2147483648.0 ? std::numeric_limits<int32_t>::min()
														   : static_cast<int32_t>(product);
		if (static_cast<int32_t>(static_cast<uint32_t>(skill->getCurrentXp()) + static_cast<uint32_t>(xpReward)) >= requiredExp) {
			skillLvl++;
			skill->setCurrentXp(0);
			skill->setSkillLvl(skillLvl);
			services::SkillLearnService::onLearnSkill(player, skillId, skillLvl, false);
		} else
			skill->setCurrentXp(static_cast<int32_t>(static_cast<uint32_t>(skill->getCurrentXp()) + static_cast<uint32_t>(xpReward)));
		return true;
	}
}

bool PlayerSkillList::isSkillPresent(int32_t skillId) {
	return skills.containsKey(skillId);
}

int32_t PlayerSkillList::getSkillLevel(int32_t skillId) {
	Ptr<PlayerSkillEntry> entry = skills.get(skillId);
	if (!entry) // Java: skills.get(skillId).getSkillLevel() on a missing skill
		throw runtime::NullPointerException("skill " + std::to_string(skillId) + " is not in the skill list");
	return entry->getSkillLevel();
}

bool PlayerSkillList::removeSkill(int32_t skillId) {
	SYNCHRONIZED(*this) {
		Ptr<PlayerSkillEntry> entry = skills.remove(skillId);
		if (!entry)
			return false;
		entry->setPersistentState(Persistable_PersistentState::DELETED);
		SYNCHRONIZED(deletedSkills) {
			deletedSkills.add(Ref<PlayerSkillEntry>(entry));
		}
		return true;
	}
}

int32_t PlayerSkillList::size() {
	return skills.size();
}

} // namespace aion::gameserver::model::skill

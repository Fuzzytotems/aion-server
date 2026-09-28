#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/skill/SkillList.h"
#include "aion/gameserver/model/skill/fwd.h"

namespace aion::gameserver::model::skill {

/**
 * The skills a player learned.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Player::skillList`), created with create. Implements the
 * erased SkillList: addSkill takes a Creature (Java: Player) and casts. The constructors only fill the map and are ported.
 *
 * @author IceReaper, orfeo087, Avol, AEJTester, Neon
 */
class PlayerSkillList final : public runtime::RefCounted, public SkillList {
	AION_MAKE_REF_FRIEND
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<PlayerSkillEntry>> skills{AION_LOCK_CLASS(PlayerSkillList::skills#stripe)};
	runtime::ArrayList<runtime::Ref<PlayerSkillEntry>> deletedSkills{AION_LOCK_CLASS(PlayerSkillList::deletedSkills)};

protected:
	PlayerSkillList();
	explicit PlayerSkillList(const std::vector<runtime::Ptr<PlayerSkillEntry>>& playerSkills);
	~PlayerSkillList() override;

public:
	/** Java: new PlayerSkillList() */
	static runtime::Ref<PlayerSkillList> create();

	/** Java: new PlayerSkillList(playerSkills) */
	static runtime::Ref<PlayerSkillList> create(const std::vector<runtime::Ptr<PlayerSkillEntry>>& playerSkills);

	/** Java: a new list of the skills */
	std::vector<runtime::Ptr<PlayerSkillEntry>> getAllSkills();

	/** Java: a new list of the deleted skills; synchronized (deletedSkills) */
	std::vector<runtime::Ptr<PlayerSkillEntry>> getDeletedSkills();

	runtime::Ptr<PlayerSkillEntry> getSkillEntry(int32_t skillId);

	/** Java: addSkill(Player player, int skillId, int skillLevel) (erased SkillList<Player>) */
	bool addSkill(gameobjects::Creature& player, int32_t skillId, int32_t skillLevel) override;

	bool addTemporarySkill(gameobjects::player::Player& player, int32_t skillId, int32_t skillLevel);

private:
	bool addSkill(gameobjects::player::Player& player, int32_t skillId, int32_t skillLevel, bool isTemporary); // synchronized

public:
	/**
	 * Only for usage with gathering and crafting skills.
	 */
	bool addSkillXp(gameobjects::player::Player& player, int32_t skillId, int32_t xpReward, int32_t objSkillLvl); // synchronized

	bool isSkillPresent(int32_t skillId) override;

	int32_t getSkillLevel(int32_t skillId) override;

	bool removeSkill(int32_t skillId) override; // synchronized, synchronized (deletedSkills)

	int32_t size() override;
};

} // namespace aion::gameserver::model::skill

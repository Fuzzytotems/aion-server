#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/skill/SkillEntry.h"
#include "aion/gameserver/model/skill/fwd.h"

namespace aion::gameserver::model::skill {

/**
 * A learned player skill (normal, stigma, profession) with its persistent state.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): the element type of PlayerSkillList. RefCounted Persistable (fieldmap K4),
 * created with create. The constructor taking the player reads the skill learn templates (static data) and stays unported; the other only
 * stores the members and is ported. `skillType` is effectively final in Java but assigned in the first constructor's body: when that
 * constructor is ported, it computes the value before delegating.
 *
 * @author ATracer, Neon
 */
class PlayerSkillEntry : public SkillEntry, public gameobjects::Persistable {
	AION_MAKE_REF_FRIEND
private:
	const int32_t skillType; // 0 normal skill , 1 stigma skill , 3 linked stigma skill
	runtime::Field<int32_t> currentXp{}; // for crafting skills
	runtime::Field<PersistentState> persistentState{};

protected:
	PlayerSkillEntry(gameobjects::player::Player& player, int32_t skillId, int32_t skillLvl, PersistentState persistentState);
	PlayerSkillEntry(int32_t skillId, int32_t skillLvl, int32_t skillType, PersistentState persistentState);
	~PlayerSkillEntry() override;

public:
	/** Java: new PlayerSkillEntry(player, skillId, skillLvl, persistentState) */
	static runtime::Ref<PlayerSkillEntry> create(gameobjects::player::Player& player, int32_t skillId, int32_t skillLvl,
		PersistentState persistentState);

	/** Java: new PlayerSkillEntry(skillId, skillLvl, skillType, persistentState) */
	static runtime::Ref<PlayerSkillEntry> create(int32_t skillId, int32_t skillLvl, int32_t skillType, PersistentState persistentState);

	bool isStigmaSkill();

	bool isNormalStigmaSkill();

	bool isLinkedStigmaSkill();

	bool isNormalSkill();

	bool isNormalOrStigmaSkill();

	bool isTappingSkill();

	bool isCraftingSkill();

	bool isMorphSkill();

	bool isProfessionSkill();

	/**
	 * Stupid NC shit: For profession skills, these values are also needed in SM_SKILL_REMOVE to be able to remove the skill from list.
	 *
	 * @return The flag that the client wants for the skill.
	 */
	int32_t getProfessionFlag();

	int32_t getFlag();

	int32_t getDateLearned();

	int32_t getSkillType() const { return skillType; }

	void setSkillLvl(int32_t skillLevel) override;

	/**
	 * This number controls the max point number for the professions skill bar.
	 */
	int32_t getProfessionSkillBarSize();

	int32_t getCurrentXp() const { return currentXp.get(); }

	void setCurrentXp(int32_t value) { currentXp.set(value); }

	PersistentState getPersistentState() override { return persistentState.get(); }

	void setPersistentState(PersistentState persistentState) override;
};

} // namespace aion::gameserver::model::skill

#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/skill/fwd.h"

namespace aion::gameserver::model::skill {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5): the interface of PlayerSkillList. Java `SkillList<T extends Creature>` is erased to
 * one interface (§8.1): the creature parameter is a Creature in every implementation.
 *
 * @author ATracer
 */
class SkillList {
public:
	/**
	 * Add skill to list
	 *
	 * @return true if operation was successful
	 */
	virtual bool addSkill(gameobjects::Creature& creature, int32_t skillId, int32_t skillLevel) = 0;

	/**
	 * Remove skill from list
	 *
	 * @return true if operation was successful
	 */
	virtual bool removeSkill(int32_t skillId) = 0;

	/**
	 * Check whether skill is present in list
	 */
	virtual bool isSkillPresent(int32_t skillId) = 0;

	virtual int32_t getSkillLevel(int32_t skillId) = 0;

	/**
	 * Size of skill list
	 */
	virtual int32_t size() = 0;

	virtual ~SkillList() = default;

protected:
	SkillList() = default;
	SkillList(const SkillList&) = default;
	SkillList& operator=(const SkillList&) = default;
};

} // namespace aion::gameserver::model::skill

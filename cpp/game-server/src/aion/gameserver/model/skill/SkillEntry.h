#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/skill/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::model::skill {

/**
 * A skill id with its level.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): the base of PlayerSkillEntry and NpcSkillEntry. Abstract RefCounted (fieldmap
 * K4). The package-private constructor only stores the members; the final getters and the level setter are ported inline.
 *
 * @author ATracer
 */
class SkillEntry : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
protected:
	const int32_t skillId;
	runtime::Field<int32_t> skillLevel{};

	/** Java package-private */
	SkillEntry(int32_t skillId, int32_t skillLevel);
	~SkillEntry() override;

public:
	int32_t getSkillId() const { return skillId; }

	int32_t getSkillLevel() const { return skillLevel.get(); }

	virtual void setSkillLvl(int32_t value) { skillLevel.set(value); }

	const skillengine::model::SkillTemplate* getSkillTemplate();
};

} // namespace aion::gameserver::model::skill

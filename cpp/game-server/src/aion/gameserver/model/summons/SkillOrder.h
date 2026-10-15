#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/summons/fwd.h"

namespace aion::gameserver::model::summons {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Summon.skillOrders`), created with create().
 *
 * @author Rolandas, Neon
 */
class SkillOrder : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t skillId;
	const int32_t skillLvl;
	const runtime::Ref<gameobjects::Creature> target;
	const int32_t hate;
	const bool release_;

protected:
	SkillOrder(int32_t skillId, int32_t skillLvl, gameobjects::Creature& target, int32_t hate, bool release);
	~SkillOrder() override;

public:
	/** Java: new SkillOrder(skillId, skillLvl, target, hate, release) */
	static runtime::Ref<SkillOrder> create(int32_t skillId, int32_t skillLvl, gameobjects::Creature& target, int32_t hate, bool release);

	int32_t getSkillId() const { return skillId; }

	int32_t getSkillLevel() const { return skillLvl; }

	runtime::Ptr<gameobjects::Creature> getTarget() const { return target; }

	int32_t getHate() const { return hate; }

	bool isRelease() const { return release_; }
};

} // namespace aion::gameserver::model::summons

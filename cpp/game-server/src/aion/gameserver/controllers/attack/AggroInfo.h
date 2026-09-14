#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers::attack {

/**
 * AggroInfo: - hate of creature - damage of creature
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4), the element type of `AggroList::aggroList`, created with
 * create(attacker). Field reads and the hate setter are ported inline.
 *
 * @author ATracer, Sarynth
 */
class AggroInfo : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	static constexpr int32_t HATE_REDUCE_VALUE = 364; // most retail npcs lose 364 hate. TODO: find formula

	const runtime::Ref<model::gameobjects::Creature> attacker;
	runtime::Field<int32_t> hate{};
	runtime::Field<int32_t> damage{};
	runtime::Field<int64_t> lastInteractionTime{0};
	runtime::Field<int32_t> hateReduceCount{1};

protected:
	/** Java package-private */
	explicit AggroInfo(model::gameobjects::Creature& attacker);
	~AggroInfo() override;

public:
	/** Java: new AggroInfo(attacker) (AggroList) */
	static runtime::Ref<AggroInfo> create(model::gameobjects::Creature& attacker);

	runtime::Ptr<model::gameobjects::Creature> getAttacker() const { return attacker; }

	void addDamage(int32_t damage);

	void addHate(int32_t hate);

	int32_t getHate() const { return hate.get(); }

	void setHate(int32_t value) { hate.set(value); }

	int32_t getDamage() const { return damage.get(); }

	int64_t getLastInteractionTime() const { return lastInteractionTime.get(); }

	/** Java package-private */
	void reduceHate();
};

} // namespace aion::gameserver::controllers::attack

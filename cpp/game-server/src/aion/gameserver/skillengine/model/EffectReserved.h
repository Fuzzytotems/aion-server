#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/skillengine/model/EffectReserved_ResourceType.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::model {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K3, `Effect::reservedEffects`), created with create().
 * Java's `this(...)` chain plus the attackStatus store become one member initializer list per constructor (const members). compareTo
 * compares positions, then Java's identity hash codes (`hashCode() - o.hashCode()`); C++ orders equal positions by object address instead
 * (only the relative order of distinct objects with the same position differs, which Java leaves unspecified as well).
 *
 * @author kecimis
 */
class EffectReserved : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	using ResourceType = EffectReserved_ResourceType;

private:
	const int32_t position;
	const int32_t value;
	const ResourceType type;
	const bool isDamage_; // Java: isDamage (renamed: clashes with isDamage())
	const bool send;
	const controllers::attack::AttackStatus attackStatus;

protected:
	EffectReserved(int32_t position, int32_t value, ResourceType type, bool isDamage);
	EffectReserved(int32_t position, int32_t value, ResourceType type, bool isDamage, bool send, controllers::attack::AttackStatus attackStatus);
	EffectReserved(int32_t position, int32_t value, ResourceType type, bool isDamage, bool send);
	~EffectReserved() override;

public:
	/** Java: new EffectReserved(position, value, type, isDamage) */
	static runtime::Ref<EffectReserved> create(int32_t position, int32_t value, ResourceType type, bool isDamage);

	/** Java: new EffectReserved(position, value, type, isDamage, send, attackStatus) */
	static runtime::Ref<EffectReserved> create(int32_t position, int32_t value, ResourceType type, bool isDamage, bool send,
		controllers::attack::AttackStatus attackStatus);

	/** Java: new EffectReserved(position, value, type, isDamage, send) */
	static runtime::Ref<EffectReserved> create(int32_t position, int32_t value, ResourceType type, bool isDamage, bool send);

	/**
	 * @return the position
	 */
	int32_t getPosition() const { return position; }

	/**
	 * @return the value
	 */
	int32_t getValue() const { return value; }

	int32_t getValueToSend();

	/**
	 * @return the type
	 */
	ResourceType getType() const { return type; }

	/**
	 * @return the isDamage
	 */
	bool isDamage() const { return isDamage_; }

	/**
	 * @return the send
	 */
	bool isSend() const { return send; }

	/**
	 * @return the attack status of this position, AttackStatus::NORMALHIT for values which cannot crit (like drained mp)
	 */
	controllers::attack::AttackStatus getAttackStatus() const { return attackStatus; }

	int32_t compareTo(const EffectReserved& o) const;
};

} // namespace aion::gameserver::skillengine::model

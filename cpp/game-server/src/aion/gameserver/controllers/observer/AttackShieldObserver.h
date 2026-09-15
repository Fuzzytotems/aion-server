#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/controllers/observer/AttackCalcObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/skillengine/model/HealType.h"
#include "aion/gameserver/skillengine/model/HitType.h"
#include "aion/gameserver/skillengine/model/ShieldType.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Shield, reflector, protect and convert-heal effects: modifies the attack results of the effected creature.
 * <p>
 * RefCounted AttackCalcObserver (fieldmap K4), created with create() by the shield effects (P5-04). `healType` is only set by the
 * ConvertHealEffect constructor; the other constructors pass Java null, so the member is a `const std::optional<HealType>` instead of the
 * fieldmap spelling `const HealType` (hub-headers.md §6: an enum Java sets to null).
 *
 * @author ATracer, Sippolo, kecimis, Luzien, Neon
 */
class AttackShieldObserver : public AttackCalcObserver {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<skillengine::model::Effect> effect;
	const skillengine::model::HitType hitType;
	const skillengine::model::ShieldType shieldType;
	const int32_t hit;
	const bool hitPercent;
	runtime::Field<int32_t> totalHit;
	const bool totalHitPercent;
	const int32_t probability;
	const int32_t minRadius;
	const int32_t maxRadius;
	const std::optional<skillengine::model::HealType> healType; // fieldmap.toml: Java passes null except for ConvertHealEffect (hub-headers.md §6)
	const int32_t mpValue;

	runtime::Field<bool> totalHitPercentSet{false};

protected:
	AttackShieldObserver(int32_t hit, int32_t totalHit, bool percent, skillengine::model::Effect& effect, skillengine::model::HitType type,
		skillengine::model::ShieldType shieldType, int32_t probability);

	AttackShieldObserver(int32_t hit, int32_t totalHit, bool percent, skillengine::model::Effect& effect, skillengine::model::HitType type,
		skillengine::model::ShieldType shieldType, int32_t probability, int32_t mpValue);

	/** healType: null unless ConvertHealEffect */
	AttackShieldObserver(int32_t hit, int32_t totalHit, bool hitPercent, bool totalHitPercent, skillengine::model::Effect& effect,
		skillengine::model::HitType type, skillengine::model::ShieldType shieldType, int32_t probability, int32_t minRadius, int32_t maxRadius,
		std::optional<skillengine::model::HealType> healType, int32_t mpValue);

	~AttackShieldObserver() override;

public:
	/** Java: new AttackShieldObserver(hit, totalHit, percent, effect, type, shieldType, probability) */
	static runtime::Ref<AttackShieldObserver> create(int32_t hit, int32_t totalHit, bool percent, skillengine::model::Effect& effect,
		skillengine::model::HitType type, skillengine::model::ShieldType shieldType, int32_t probability);

	/** Java: new AttackShieldObserver(hit, totalHit, percent, effect, type, shieldType, probability, mpValue) */
	static runtime::Ref<AttackShieldObserver> create(int32_t hit, int32_t totalHit, bool percent, skillengine::model::Effect& effect,
		skillengine::model::HitType type, skillengine::model::ShieldType shieldType, int32_t probability, int32_t mpValue);

	/** Java: the 12-argument constructor */
	static runtime::Ref<AttackShieldObserver> create(int32_t hit, int32_t totalHit, bool hitPercent, bool totalHitPercent,
		skillengine::model::Effect& effect, skillengine::model::HitType type, skillengine::model::ShieldType shieldType, int32_t probability,
		int32_t minRadius, int32_t maxRadius, std::optional<skillengine::model::HealType> healType, int32_t mpValue);

	/** attackerEffect: null for auto attacks */
	void checkShield(const std::vector<runtime::Ptr<attack::AttackResult>>& attackList, runtime::Ptr<skillengine::model::Effect> attackerEffect,
		model::gameobjects::Creature& attacker) override;

private:
	/** effect: nullable */
	bool isPunchShield(runtime::Ptr<skillengine::model::Effect> effect);

public:
	skillengine::model::ShieldType getShieldType() const { return shieldType; }
};

} // namespace aion::gameserver::controllers::observer

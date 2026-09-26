#pragma once

#include <cstdint>
#include <unordered_set>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::model {
class Effect_ForceType; // Java Effect.ForceType (skillengine/model/Effect_ForceType.h)
} // namespace aion::gameserver::skillengine::model

namespace aion::gameserver::controllers::effect {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The effect controller part of players (`setEffectController(
 * std::make_unique<PlayerEffectController>(*this))`), an OwnedPart bound in the EffectController constructor. getOwner() is Java's cast-only
 * override, a non-virtual narrowing accessor (§8.2). Both removeAllEffects overloads are overridden, so no using-declaration is needed.
 *
 * @author ATracer
 */
class PlayerEffectController : public EffectController {
private:
	runtime::EnumMap<CumulativeResistType, runtime::Ref<CumulativeResist>> cumulativeResistInfo{
		AION_LOCK_CLASS(PlayerEffectController::cumulativeResistInfo)};
	runtime::Field<bool> keepBuffsOnDie{};

public:
	explicit PlayerEffectController(model::gameobjects::Creature& owner);
	~PlayerEffectController() override;

	void setKeepBuffsOnDie(bool value) { keepBuffsOnDie.set(value); }

	void addEffect(skillengine::model::Effect& effect) override;

	void clearEffect(skillengine::model::Effect& effect, bool broadcast) override;

	/** Narrowing accessor (Java: Player getOwner() returning (Player) super.getOwner(), hub-headers.md §8.2). */
	model::gameobjects::player::Player& getOwner() const;

	void removeAllEffects(bool logout) override;

	/**
	 * Removes non-storable effects and their conditional effects (like Aethertech buffs)
	 */
	void removeNonStorableEffectsForLogout();

private:
	/** @param effect null updates all slots (removeAllEffects) */
	void updatePlayerIconsAndGroup(runtime::Ptr<skillengine::model::Effect> effect);

public:
	/** @param effect null for all slots */
	void updatePlayerEffectIcons(runtime::Ptr<skillengine::model::Effect> effect);

private:
	/**
	 * Effect of DEBUFF should not be added if duel ended (friendly unit)
	 */
	bool checkDuelCondition(skillengine::model::Effect& effect);

public:
	/** @param magicalCriticalPositions nullable (Java null), passed on to Effect */
	void addSavedEffect(int32_t skillId, int32_t skillLvl, int32_t remainingTime, int64_t endTime,
		const skillengine::model::Effect_ForceType* forceType, const std::unordered_set<int32_t>* magicalCriticalPositions);

	// synchronized (cumulativeResistInfo)
	void removeAllEffects() override;

protected:
	bool canRemoveOnDie(skillengine::model::Effect& effect) override;

public:
	// synchronized (cumulativeResistInfo)
	int64_t calculateAndApplyCumulativeResistDuration(CumulativeResistType type, int64_t duration);

	// synchronized (cumulativeResistInfo)
	int32_t getCumulativeResistance(CumulativeResistType type);
};

} // namespace aion::gameserver::controllers::effect

#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::danuarSanctuary {

/**
 * The Danuar Sanctuaries' siege drill ("siege_drill"): for a Danuar key (185000174) it drives to the barricade 233189 and bores it.
 * <p>
 * Java: data/handlers/ai/instance/danuarSanctuary/SiegeDrill.java, @AIName("siege_drill") (the marker is in the .cpp).
 *
 * @author xTz
 */
class SiegeDrill : public NpcAI {
	friend struct SiegeDrill_ItemUseObserver;

protected:
	runtime::Field<int32_t> startBarAnimation{1};
	runtime::Field<int32_t> cancelBarAnimation{2};

private:
	runtime::AtomicBoolean isUsed{AION_LOCK_CLASS(SiegeDrill::isUsed), false}; // Java: = new AtomicBoolean(false)

public:
	explicit SiegeDrill(Npc& owner) : NpcAI(owner) {}

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

protected:
	void handleDialogStart(Player& player) override;

	void handleMoveArrived() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

	virtual void handleUseItemStart(Player& player);

	virtual void handleUseItemFinish(Player& player);

	virtual int32_t getTalkDelay();
};

} // namespace aion::gameserver::handlers::ai::instance::danuarSanctuary

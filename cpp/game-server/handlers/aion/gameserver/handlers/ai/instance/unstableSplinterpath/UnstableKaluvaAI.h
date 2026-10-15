#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

/**
 * The AI of Kaluva in the Unstable Splinterpath ("unstablekaluva"): an attack has a 3 % chance to send her, not thinking, to one of her
 * four eggs with 19152; arrived she takes the egg's 19222, casts 19223 on it, loses 19152 and 2 s later fights her most hated again; no
 * loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/unstableSplinterpath/UnstableKaluvaAI.java, @AIName("unstablekaluva") (the marker is in the .cpp).
 *
 * @author Luzien, Cheatkiller
 */
class UnstableKaluvaAI : public AggressiveNpcAI {
private:
	runtime::Field<bool> canThink_{true}; // Java: canThink (the name clashes with canThink())
	runtime::Field<bool> isInMove{false};

public:
	explicit UnstableKaluvaAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void moveToSpawner(int32_t egg);

protected:
	void handleMoveArrived() override;

	void handleBackHome() override;

private:
	int32_t randomEgg();

public:
	bool canThink() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath

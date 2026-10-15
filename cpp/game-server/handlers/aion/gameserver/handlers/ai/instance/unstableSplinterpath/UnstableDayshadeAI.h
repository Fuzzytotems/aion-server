#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

/**
 * The AI of Dayshade in the Unstable Splinterpath ("unstabledayshade"): the first attack makes it die into Ebonsoul and Rukril; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/unstableSplinterpath/UnstableDayshadeAI.java, @AIName("unstabledayshade") (the marker is in the .cpp).
 *
 * @author Luzien, Ritsu, Cheatkiller
 */
class UnstableDayshadeAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(UnstableDayshadeAI::isHome), true}; // Java: = new AtomicBoolean(true)

public:
	explicit UnstableDayshadeAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

	void handleBackHome() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath

#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

/**
 * The AI of Dayshade in the Abyssal Splinter ("dayshade"): the first attack makes it die into Ebonsoul and Rukril; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/abyssal_splinter/DayshadeAI.java, @AIName("dayshade") (the marker is in the .cpp).
 *
 * @author Luzien, Ritsu
 */
class DayshadeAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(DayshadeAI::isHome), true}; // Java: = new AtomicBoolean(true)

public:
	explicit DayshadeAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

	void handleBackHome() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter

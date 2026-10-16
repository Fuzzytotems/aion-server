#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

/**
 * Takun and Gojira, the twins of Empyrean Crucible's stage 9 ("takun_gojira"): half a second after its spawn each twin hates the other.
 * <p>
 * Java: data/handlers/ai/instance/empyreanCrucible/TakunGojiraAI.java, @AIName("takun_gojira") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class TakunGojiraAI : public AggressiveNpcAI {
private:
	runtime::Field<runtime::Ref<Npc>> counterpart{};

public:
	explicit TakunGojiraAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible

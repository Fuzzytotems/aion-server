#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/SummonerAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

/**
 * The AI of Kaluva the Fourth Fragment in the Abyssal Splinter ("kaluva"): her individual summons percentage sends her, not thinking, to
 * an egg she spawns at one of four places; arrived she casts on it and 2 s later fights her most hated again; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/abyssal_splinter/KaluvaAI.java, @AIName("kaluva") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class KaluvaAI : public SummonerAI {
private:
	runtime::Field<bool> canThink_{true}; // Java: canThink (the name clashes with canThink())

public:
	explicit KaluvaAI(Npc& owner) : SummonerAI(owner) {}

protected:
	void handleIndividualSpawnedSummons(const model::templates::ai::Percentage& percent) override;

	void handleMoveArrived() override;

private:
	runtime::Ptr<VisibleObject> spawn();

public:
	bool canThink() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter

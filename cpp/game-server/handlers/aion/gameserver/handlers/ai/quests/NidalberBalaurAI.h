#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::quests {

/**
 * The AI of the Nidalber balaur ("nidalber_balaur"): in the two quest maps it walks to the quest npc (204044 or 204432) and attacks it,
 * and goes when that npc is missing or dead.
 * <p>
 * Java: data/handlers/ai/quests/NidalberBalaurAI.java, @AIName("nidalber_balaur") (the marker is in the .cpp).
 *
 * @author Yeats, Pad
 */
class NidalberBalaurAI : public AggressiveNpcAI {
private:
	runtime::Field<runtime::Ref<Npc>> questNpc{};

public:
	explicit NidalberBalaurAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void setNewSpawnPosition();

	void findQuestNpc();

	void moveToQuestNpc();

public:
	void handleNotAtHome() override;

protected:
	void handleBackHome() override;

public:
	void handleMoveArrived() override;

private:
	void addHate();
};

} // namespace aion::gameserver::handlers::ai::quests

#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

/**
 * The AI of Harlequin Lord Reshka's summons ("harlequinlordreshkasummon"): spawned while Reshka lives, it hates his most hated (Java's
 * addHate(null) of an empty aggro list does nothing).
 * <p>
 * Java: data/handlers/ai/instance/nightmareCircus/HarlequinLordReshkaSummonAI.java, @AIName("harlequinlordreshkasummon") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class HarlequinLordReshkaSummonAI : public AggressiveNpcAI {
public:
	explicit HarlequinLordReshkaSummonAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus

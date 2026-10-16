#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

/**
 * Empyrean Crucible's record keepers ("empyreanrecordkeeper"): each announces itself, starts its stage or elevator through the instance
 * handler and goes; 799567 can also start at stage 7, 205344 gives the score.
 * <p>
 * Java: data/handlers/ai/instance/empyreanCrucible/EmpyreanRecordKeeperAI.java, @AIName("empyreanrecordkeeper") (the marker is in the .cpp).
 *
 * @author xTz, Luzien
 */
class EmpyreanRecordKeeperAI : public NpcAI {
public:
	explicit EmpyreanRecordKeeperAI(Npc& owner) : NpcAI(owner) {}

	void handleSpawned() override;

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible

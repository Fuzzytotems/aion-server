#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Tahabata's last altar in Tiamat Stronghold ("tahabataaltar2"): it casts 20972 every 2 s from its spawn. No decay, respawn or
 * rewards.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/TahabataAltarFinalAI.java, @AIName("tahabataaltar2") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class TahabataAltarFinalAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> task{};

public:
	explicit TahabataAltarFinalAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	void handleDespawned() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold

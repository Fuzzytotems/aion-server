#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Surama the Traitor in Tiamat Stronghold ("suramathetraitor"): spawned she walks to Raksha; 10 s later they speak, and 8 s after
 * that Raksha (219356) casts 20952 on her and becomes attackable; her death speaks once more.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/SuramaTheTraitorAI.java, @AIName("suramathetraitor") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class SuramaTheTraitorAI : public GeneralNpcAI {
public:
	explicit SuramaTheTraitorAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleDied() override;

private:
	void moveToRaksha();

	void startDialog();
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold

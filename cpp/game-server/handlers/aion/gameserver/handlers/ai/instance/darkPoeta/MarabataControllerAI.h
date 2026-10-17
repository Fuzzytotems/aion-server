#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

/**
 * The boosters of Dark Poeta's Marabatas ("marabata_controller"): each casts its skill after 3.5 s and buffs its Marabata after 10 s;
 * its death takes the buff back.
 * <p>
 * Java: data/handlers/ai/instance/darkPoeta/MarabataControllerAI.java, @AIName("marabata_controller") (the marker is in the .cpp).
 *
 * @author xTz
 */
class MarabataControllerAI : public NpcAI {
public:
	explicit MarabataControllerAI(Npc& owner) : NpcAI(owner) {}

private:
	runtime::Ptr<Npc> getBoss();

	void applyEffect(bool remove);

protected:
	void handleDied() override;

	void handleSpawned() override;

private:
	void useSkill();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta

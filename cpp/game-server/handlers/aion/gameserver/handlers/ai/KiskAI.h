#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a kisk ("kisk"): its owner's legion, group or alliance binds to it with a question; attacks, its removal and its destruction are
 * announced around it.
 * <p>
 * Java: data/handlers/ai/KiskAI.java, @AIName("kisk") (the marker is in the .cpp). Java's anonymous AIRequest is the callback struct
 * KiskAI_AIRequest of the .cpp (fieldmap ai.KiskAI$1).
 *
 * @author ATracer, Source
 */
class KiskAI : public NpcAI {
public:
	explicit KiskAI(Npc& owner) : NpcAI(owner) {}

	/** Java: getOwner() narrowed to Kisk (KiskAI.java:28-31) */
	Kisk& getKiskOwner() const;

	bool ask(AIQuestion question) override;

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;
	void handleDespawned() override;
	void handleDialogStart(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai

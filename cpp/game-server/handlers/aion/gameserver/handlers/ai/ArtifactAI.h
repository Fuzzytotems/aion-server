#pragma once

#include "aion/commons/logging/Logger.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a siege artifact ("artifact"): a click opens the activation window (with the artifact's cooldown), then the confirmation with the
 * item cost; an authorized activation takes 10 s of channeling (an ItemUseObserver aborts it), consumes the items and casts the artifact's
 * skill on the creatures of its location 13 s later, once or every repeat interval. Java's anonymous classes are the callback structs of the
 * .cpp (fieldmap ai.ArtifactAI$1..$7); the inner class ArtifactUseSkill is ArtifactAI_ArtifactUseSkill.
 * <p>
 * Java: data/handlers/ai/ArtifactAI.java, @AIName("artifact") (the marker is in the .cpp). M5i I-3, under P5-12a's lease (chunks.cmake).
 *
 * @author ATracer, Source
 */
class ArtifactAI : public gameserver::ai::NpcAI {
public:
	explicit ArtifactAI(Npc& owner) : gameserver::ai::NpcAI(owner) {}

	void onActivate(Player& player);

protected:
	/** Java's covariant override of getSpawnTemplate: the spawn narrowed to a SiegeSpawnTemplate (ClassCastException otherwise) */
	runtime::Ptr<SiegeSpawnTemplate> getSpawnTemplate();

	void handleDialogStart(Player& player) override;

	void handleDialogFinish(Player& player) override;

private:
	/** Java: LoggerFactory.getLogger("SIEGE_LOG") */
	static const commons::logging::Logger log;

	friend struct ArtifactAI_AIRequest;
	friend struct ArtifactAI_AIRequest_2;
	friend struct ArtifactAI_ItemUseObserver;
	friend struct ArtifactAI_ArtifactUseSkill;
};

} // namespace aion::gameserver::handlers::ai

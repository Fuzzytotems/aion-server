#pragma once

#include <cstdint>

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/OneDmgNoActionAI.h"

namespace aion::gameserver::handlers::ai::worlds::eltnen {

/**
 * The AI of the Mysterious Crate in Eltnen ("dracusbox"): its death spawns, where it stood, an Elroco (211792), an Oozing Clodworm (211799)
 * or Chaos Dracus (211800) at even odds and deletes the crate at once; the crate's respawn comes at once, or with Chaos Dracus's death.
 * It never respawns otherwise.
 * <p>
 * Java: data/handlers/ai/worlds/eltnen/DracusBox.java, @AIName("dracusbox") (the marker is in the .cpp).
 *
 * @author Neon
 */
class DracusBox : public OneDmgNoActionAI {
private:
	static constexpr int32_t DRACUS_ID = 211800;

public:
	explicit DracusBox(Npc& owner) : OneDmgNoActionAI(owner) {}

protected:
	void handleDied() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::worlds::eltnen

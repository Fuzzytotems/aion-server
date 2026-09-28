#pragma once

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/handler/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::ai::handler {

/**
 * Handles the dialog events of an NPC: a creature started talking to it, and the dialog window it answers with.
 * <p>
 * A static-only utility class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class TalkEventHandler {
public:
	TalkEventHandler() = delete;

	static void onTalk(NpcAI& npcAI, model::gameobjects::Creature& creature);

	static void onSimpleTalk(NpcAI& npcAI, model::gameobjects::Creature& creature);

	static void onFinishTalk(NpcAI& npcAI, model::gameobjects::Creature& creature);
};

} // namespace aion::gameserver::ai::handler

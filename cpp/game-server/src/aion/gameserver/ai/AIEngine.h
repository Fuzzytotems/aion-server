#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/model/GameEngine.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::handlers {
struct AIHandlerEntry;
} // namespace aion::gameserver::handlers

namespace aion::gameserver::ai {

/**
 * Creates the AIs of creatures from the AI handler registry.
 * <p>
 * Engine spine header (docs/design/hub-headers.md §3.5, handlers-and-porting-plan.md §1.6/§1.7). An Immortal singleton (§11.2). C++
 * differences:
 * - Java's ScriptManager and the `aiHandlers` map of classes have no member: the AI handler registry (HandlerRegistry.h aiHandlerEntries(),
 *   written by aion_gs_regscan from the AION_AI markers) replaces them; registerAI takes a registry entry instead of a Class.
 * - findConstructor and findDefaultOwnerType (reflection) are not declared: the AION_AI marker checks the owner type at compile time
 *   (HandlerRegistry.h AIHandlerClass), and the factory returns nullptr when the owner's dynamic type does not fit.
 * - newAI is ported (Creature::postConstruct needs it): a null name creates DummyAI (an AITemplate over Creature, defined in AIEngine.cpp),
 *   otherwise the registry factory creates the AI and newAI stores the entry for AbstractAI::getName().
 * - The C++-only setting gameserver.dev.missing_ai_handlers=warn turns a missing handler into a DummyAI and a warning (isMissingAiHandlersWarn).
 *   For an Npc owner the substitute is a DummyNpcAI, so that the Java-faithful `(NpcAI) npc.getAi()` casts keep working (D15 below).
 *
 * @author ATracer
 */
class AIEngine : public runtime::Immortal, public model::GameEngine {
	// fieldmap.toml drops scriptManager and aiHandlers (replaced by HandlerRegistry.h)
private:
	/** Java: private static class DummyAI<T extends Creature> extends AITemplate<T> (defined in AIEngine.cpp, §9.3) */
	template <class T>
	class DummyAI;

	/**
	 * C++ only (m5b-plan.md D15, docs/deviations/P4-01.md): the warn-mode substitute for an Npc whose AI name has no handler. It is a
	 * DummyAI that happens to derive NpcAI, so `runtime::cast<NpcAI>(npc.getAi())` - a faithful port of Java's `(NpcAI) npc.getAi()`, which
	 * cannot fail in Java because newAI throws for an unregistered name - keeps working. Every NpcAI hook is overridden back to the
	 * AITemplate no-op and ask() back to all-false, so the behaviour is exactly a DummyAI's; only the static type differs. Defined in
	 * AIEngine.cpp.
	 */
	class DummyNpcAI;

	AIEngine();
	~AIEngine();

public:
	void init() override;

	void reload();

	/** Java: registerAI(Class<AbstractAI<? extends Creature>> aiClass), called by AIHandlerClassListener; C++: one registry entry */
	void registerAI(const handlers::AIHandlerEntry& aiClass);

	/**
	 * Java: public <T extends Creature> AbstractAI<? extends Creature> newAI(String name, T owner)
	 *
	 * @param name the AI name (@AIName), nullopt for a DummyAI
	 * @return the new AI part for owner (the caller stores it into the creature's `ai` PartSlot)
	 * @throws IllegalArgumentException if no AI has that name, the AI does not accept the owner's type, or its constructor failed
	 */
	std::unique_ptr<AbstractAI> newAI(std::optional<std::string_view> name, model::gameobjects::Creature& owner);

private:
	void validateScripts();

public:
	/**
	 * C++ only: true if AIConfig::MISSING_AI_HANDLERS is "warn" (gameserver.dev.missing_ai_handlers, docs/deviations/P4-01.md). Then
	 * validateScripts logs NPC template AI names without a handler instead of failing, and newAI creates a DummyAI for such a name (one warning
	 * per name) instead of throwing.
	 */
	static bool isMissingAiHandlersWarn();

	/** C++ only: the AI names newAI replaced by a DummyAI so far, sorted (startup summary and tests) */
	static std::vector<std::string> missingAiNamesSeen();

	static AIEngine& getInstance();
};

} // namespace aion::gameserver::ai

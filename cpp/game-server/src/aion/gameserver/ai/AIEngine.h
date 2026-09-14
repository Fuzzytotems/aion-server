#pragma once

#include <memory>
#include <optional>
#include <string_view>

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
 *
 * @author ATracer
 */
class AIEngine : public runtime::Immortal, public model::GameEngine { // fieldmap: no scriptManager, aiHandlers (replaced by HandlerRegistry.h)
private:
	/** Java: private static class DummyAI<T extends Creature> extends AITemplate<T> (defined in AIEngine.cpp, §9.3) */
	template <class T>
	class DummyAI;

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
	static AIEngine& getInstance();
};

} // namespace aion::gameserver::ai

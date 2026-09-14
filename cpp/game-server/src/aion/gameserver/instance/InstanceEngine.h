#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/instance/fwd.h"
#include "aion/gameserver/instance/handlers/fwd.h"
#include "aion/gameserver/model/GameEngine.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::handlers {
struct InstanceHandlerEntry;
} // namespace aion::gameserver::handlers

namespace aion::gameserver::instance {

/**
 * Creates the instance handlers of world map instances from the instance handler registry.
 * <p>
 * Engine spine header (docs/design/hub-headers.md §3.5, handlers-and-porting-plan.md §1.6). An Immortal singleton (§11.2). Java's
 * ScriptManager and the `instanceHandlers` map of classes have no member: the instance handler registry (HandlerRegistry.h
 * instanceHandlerEntries(), from the AION_INSTANCE_HANDLER markers) replaces them, so addInstanceHandlerClass takes a registry entry.
 * getNewInstanceHandler returns the new RefCounted handler (Java: the registered class or a GeneralInstanceHandler).
 *
 * @author ATracer
 */
class InstanceEngine : public runtime::Immortal, public model::GameEngine { // fieldmap: no instanceHandlers (replaced by HandlerRegistry.h)
private:
	InstanceEngine();
	~InstanceEngine();

public:
	void init() override;

	runtime::Ref<handlers::InstanceHandler> getNewInstanceHandler(world::WorldMapInstance& instance);

	/** Java: final void addInstanceHandlerClass(Class<? extends InstanceHandler> handler); C++: one registry entry */
	void addInstanceHandlerClass(const gameserver::handlers::InstanceHandlerEntry& handler);

	static InstanceEngine& getInstance();
};

} // namespace aion::gameserver::instance

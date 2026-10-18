#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/instance/fwd.h"
#include "aion/gameserver/instance/handlers/fwd.h"
#include "aion/gameserver/model/GameEngine.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::instance {

/**
 * Creates the instance handlers of world map instances from the instance handler registry.
 * <p>
 * Engine spine header (docs/design/hub-headers.md §3.5, handlers-and-porting-plan.md §1.6). An Immortal singleton (§11.2). Java's
 * ScriptManager and the `instanceHandlers` map of classes have no member: the instance handler registry (HandlerRegistry.h
 * instanceHandlerEntries(), from the AION_INSTANCE_HANDLER markers) replaces them. getNewInstanceHandler returns the new RefCounted handler
 * (Java: the registered class or a GeneralInstanceHandler).
 * <p>
 * Java's addInstanceHandlerClass is omitted (m5f-plan.md D10, the instance chunk's deviations). It has no caller: in Java, the
 * InstanceHandlerClassListener of the script manager called it to fill the map, once per loaded @InstanceID class. Here the registry is
 * compiled in, and each marker is its entry.
 *
 * @author ATracer
 */
class InstanceEngine : public runtime::Immortal, public model::GameEngine {
	// fieldmap.toml drops instanceHandlers (replaced by HandlerRegistry.h)
private:
	InstanceEngine();
	~InstanceEngine();

public:
	void init() override;

	runtime::Ref<handlers::InstanceHandler> getNewInstanceHandler(world::WorldMapInstance& instance);

	static InstanceEngine& getInstance();
};

} // namespace aion::gameserver::instance

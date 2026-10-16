#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance {

/**
 * Lower Udas Temple (300160000): the named drop chests at creation; Zhanim the Librarian and the 36 ancient traps at the first entry,
 * the traps gone when an item use finishes.
 * <p>
 * Java: data/handlers/instance/LowerUdasTempleInstance.java.
 *
 * @author xTz, Luzien, Majka
 */
class LowerUdasTempleInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<runtime::Ref<Npc>> traps{AION_LOCK_CLASS(LowerUdasTempleInstance::traps)}; // Java: = new ArrayList<>()
	runtime::AtomicBoolean wasSpawned{AION_LOCK_CLASS(LowerUdasTempleInstance::wasSpawned)}; // Java: = new AtomicBoolean()

	/** Java: private final static WorldPosition[] trap_positions (built once, immortal like the class's static array) */
	static const runtime::Ref<runtime::Array<runtime::Ref<WorldPosition>>>& trap_positions();

protected:
	explicit LowerUdasTempleInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<LowerUdasTempleInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

	void onEnterInstance(Player& player) override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance

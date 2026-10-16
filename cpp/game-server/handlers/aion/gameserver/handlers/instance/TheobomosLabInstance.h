#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance {

/**
 * Theobomos Lab (310110000): Triroan's death opens the exit (730178). Silikor's two guards (280971, 280972) must die within 7 m of the orb
 * (280973); a guard dying elsewhere comes back where it fell. When both died there, 1 s later Silikor (214668) loses 18481 and the orb goes.
 * Half of the time a Dimensional Researcher (798223) at one of three places. Npcs with a player master (summons) are ignored.
 * <p>
 * Java: data/handlers/instance/TheobomosLabInstance.java.
 *
 * @author xTz, Ritsu, Tibald
 */
class TheobomosLabInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<bool> isInstanceDestroyed{};
	runtime::Field<bool> isDead1{}; // Java: = false
	runtime::Field<bool> isDead2{}; // Java: = false

protected:
	explicit TheobomosLabInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<TheobomosLabInstance> create(WorldMapInstance& instance);

	void onDie(Npc& npc) override;

	void onInstanceDestroy() override;

private:
	bool guardDie(Npc& npc);

	void removeBuff();

public:
	void onInstanceCreate() override;
};

} // namespace aion::gameserver::handlers::instance

#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //fixpath: fixes the Z-coordinates of an NPC walk route with the GM's client (not the internal geo data) and writes the route with
 * WalkerData.saveData (m5j CP5, D14 = (a)).
 * <p>
 * C++: Java's `private static volatile Player runner` and `private static boolean oldInvul` are the fields of one immortal FixPath::State
 * (state()); the route runs on a long-running pool task (Java: ThreadPoolManager.execute, "run in a new thread"), because getZ blocks it on the
 * wait task's future for up to five seconds per step.
 */
class FixPath : public AdminCommand {
public:
	FixPath();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	/** Java: the static fields runner and oldInvul */
	struct State {
		runtime::Field<runtime::Ref<Player>> runner{};
		runtime::Field<bool> oldInvul{false};
	};

	static State& state();

	/** Java: getZ's float lastZ[], shared with its wait task */
	class LastZ : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	public:
		runtime::Field<float> value{0.0f};

	protected:
		LastZ() = default;
		~LastZ() override = default;
	};

	/** @return Returns the Z coordinate of the player after he stopped moving, or 0 on timeout. */
	float getZ(Player& admin);

	void stop();
};

} // namespace aion::gameserver::handlers::admincommands

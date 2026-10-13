#include "aion/gameserver/handlers/admincommands/FixPath.h"

#include <memory>
#include <vector>

#include "aion/commons/configuration/transformers/NumberTransformer.h"
#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sched/TimeUnit.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(FixPath);

// parity= private static volatile Player runner = null; private static boolean oldInvul = false; (FixPath::State)

// Java FixPath.java:29-36
FixPath::FixPath()
	: AdminCommand("fixpath", "Fixes Z-coordinates for NPC walk routes using your client (not internal geo data).",
		  "[z-offset] - Gathers new Z-coordinates for the route of your target (default: uses old Z values as a base, optional: adds the offset to old values).\n"
		  "<route ID> [z-offset] - Gathers new Z-coordinates for the specified route (default: uses old Z values as a base, optional: adds the offset to old values).\n"
		  "cancel - Cancels route fixing.\n") {
}

FixPath::State& FixPath::state() { // parity: the accessor of the static fields
	static State* const fields = new State(); // parity: the static fields (immortal)
	return *fields;
}

// Java FixPath.java:38-140
void FixPath::execute(Player& admin, std::span<const std::string> params) {
	using model::templates::walker::RouteStep; // parity: C++ using-declaration
	using model::templates::walker::WalkerTemplate; // parity: C++ using-declaration
	using commons::configuration::transformers::NumberParser::parseFloat; // parity: C++ using-declaration
	if (params.empty() && runtime::as<Npc>(admin.getTarget()) == nullptr) { // parity= if (params.length == 0 && !(admin.getTarget() instanceof Npc)) {
		sendInfo(admin);
		return;
	}

	if (runtime::Ref<Player> runner = state().runner.get(); runner != nullptr) { // parity= if (runner != null) {
		if (!admin.equals(*runner)) { // parity= if (!admin.equals(runner)) {
			sendInfo(admin, "Someone is already running this command!");
		} else if ((params.empty() ? throw runtime::IndexOutOfBoundsException("Index 0 out of bounds for length 0") : params[0]) == "cancel") { // parity= } else if ("cancel".equals(params[0])) {
			sendInfo(admin, "Canceled path fixing.");
			stop();
		} else {
			sendInfo(admin);
		}
		return;
	}

	int32_t map = 0;
	float zOffset = 0.1f;
	const WalkerTemplate* t = nullptr; // parity= WalkerTemplate t = null;

	if (!params.empty() && (t = DataManager::WALKER_DATA->getWalkerTemplate(params[0])) != nullptr) { // parity= if (params.length > 0 && (t = DataManager.WALKER_DATA.getWalkerTemplate(params[0])) != null) {
		if (params.size() > 1)
			zOffset = parseFloat(params[1]); // parity= zOffset = Float.parseFloat(params[1]);
		map = admin.getWorldId();
		sendInfo(admin, "Make sure you are on the correct map. If not use <cancel>!");
	} else {
		if (runtime::as<Npc>(admin.getTarget()) != nullptr) { // parity= if (admin.getTarget() instanceof Npc) {
			if (!params.empty()) // parity= if (params.length > 0)
				zOffset = parseFloat(params[0]); // parity= zOffset = Float.parseFloat(params[0]);
			map = admin.getTarget()->getWorldId();
			std::optional<std::string> walkerId = admin.getTarget()->getSpawn()->getWalkerId(); // parity= t = DataManager.WALKER_DATA.getWalkerTemplate(admin.getTarget().getSpawn().getWalkerId());
			t = walkerId ? DataManager::WALKER_DATA->getWalkerTemplate(*walkerId) : nullptr; // parity: (the same; a null id is no route)
		}
		if (t == nullptr) {
			sendInfo(admin, "Couldn't find route ID.");
			return;
		}
	}

	state().runner.set(runtime::Ref<Player>(admin)); // parity= runner = admin;
	state().oldInvul.set(admin.isInvulnerable()); // parity= oldInvul = admin.isInvulnerable();
	const WalkerTemplate* const templ = t; // parity= final WalkerTemplate template = t;
	const int32_t worldId = map;
	const float zOff = zOffset;

	runtime::Ref<Player> adminRef(admin); // parity: the lambda's capture of admin
	// run in a new thread to avoid blocking everything (C++: a long-running task, getZ blocks it)
	utils::ThreadPoolManager::getInstance().executeLongRunning({this, &admin}, [this, adminRef, templ, worldId, zOff] { // parity= ThreadPoolManager.getInstance().execute(() -> { // run in a new thread to avoid blocking everything
		Player& admin = *adminRef; // parity: (the capture)
		admin.setCustomState(CustomPlayerState::INVULNERABLE);
		const RouteStep* start = templ->getRouteStep(0); // parity= RouteStep start = template.getRouteStep(0);
		TeleportService::teleportTo(admin, *world::WorldPosition::create(worldId, start->getX(), start->getY(), start->getZ() + zOff, admin.getHeading())); // parity= TeleportService.teleportTo(admin, new WorldPosition(worldId, start.getX(), start.getY(), start.getZ() + zOff, admin.getHeading()));
		float zDelta = getZ(admin) - start->getZ() + zOff;

		std::vector<float> corrections; // parity= List<Float> corrections = new ArrayList<>(template.getRouteSteps().size());
		struct StopAtEnd { // parity: Java's finally { stop(); } below
			FixPath* command; // parity: (the same)
			~StopAtEnd() { command->stop(); } // parity: (the same)
		} stopAtEnd{this}; // parity: Java's finally (RAII; the waiver at the end of the task)
		const int32_t stepCount = static_cast<int32_t>(templ->getRouteSteps().size()); // parity: template.getRouteSteps().size(), below
		int32_t maxNonWalkBackStep = templ->getLoopType() == WalkerTemplate::LoopType::WALK_BACK ? (stepCount + 1) / 2 // parity= int maxNonWalkBackStep = template.getLoopType() == LoopType.WALK_BACK ? (template.getRouteSteps().size() + 1) / 2
			: stepCount; // parity= : template.getRouteSteps().size();
		for (const std::unique_ptr<RouteStep>& step : templ->getRouteSteps()) { // parity= for (RouteStep step : template.getRouteSteps()) {
			if (state().runner.get() == nullptr) // parity= if (runner == null) // on cancel
				return;
			if (step->getStepIndex() > maxNonWalkBackStep) // walk back steps are added in afterUnmarshal, not in templates
				break;
			sendInfo(admin, "Teleporting to step " + std::to_string(step->getStepIndex()) + "..."); // parity= sendInfo(admin, "Teleporting to step " + step.getStepIndex() + "...");
			TeleportService::teleportTo(admin, // parity= TeleportService.teleportTo(admin,
				*world::WorldPosition::create(admin.getWorldId(), step->getX(), step->getY(), step->getZ() + zDelta, admin.getHeading())); // parity= new WorldPosition(admin.getWorldId(), step.getX(), step.getY(), step.getZ() + zDelta, admin.getHeading()));
			corrections.push_back(getZ(admin)); // parity= corrections.add(getZ(admin));
		}

		sendInfo(admin, "Saving and applying corrections...");

		dataholders::WalkerData data; // parity= WalkerData data = new WalkerData();
		auto newTemplate = std::make_unique<WalkerTemplate>(templ->getRouteId()); // parity= WalkerTemplate newTemplate = new WalkerTemplate(template.getRouteId());
		std::vector<std::unique_ptr<RouteStep>> newSteps; // parity= List<RouteStep> newSteps = new ArrayList<>();

		for (int32_t stepIndex = 0; stepIndex < static_cast<int32_t>(corrections.size()); stepIndex++) { // parity= for (int stepIndex = 0; stepIndex < corrections.size(); stepIndex++) {
			float fixedZ = corrections[static_cast<size_t>(stepIndex)]; // parity= float fixedZ = corrections.get(stepIndex);
			const RouteStep* oldStep = templ->getRouteStep(stepIndex); // parity= RouteStep oldStep = template.getRouteStep(stepIndex);
			newSteps.push_back(std::make_unique<RouteStep>(oldStep->getX(), oldStep->getY(), fixedZ, oldStep->getRestTime())); // parity= newSteps.add(new RouteStep(oldStep.getX(), oldStep.getY(), fixedZ, oldStep.getRestTime()));
			oldStep->setZ(fixedZ); // live fixing
			if (templ->getLoopType() == WalkerTemplate::LoopType::WALK_BACK && stepIndex > 0 && stepIndex < maxNonWalkBackStep) // live fixing of walk back steps
				templ->getRouteStep(stepCount - stepIndex)->setZ(fixedZ); // parity= template.getRouteStep(template.getRouteSteps().size() - stepIndex).setZ(fixedZ);
		}

		newTemplate->setRouteSteps(std::move(newSteps)); // parity= newTemplate.setRouteSteps(newSteps);
		newTemplate->setLoopType(templ->getLoopType());
		newTemplate->setPool(templ->getPool());
		newTemplate->setType(templ->getType());
		newTemplate->setRows(templ->getRows());
		data.addTemplate(std::move(newTemplate)); // parity= data.addTemplate(newTemplate);
		data.saveData(templ->getRouteId());

		sendInfo(admin, "Finished path fixing.");
		// parity= } finally { stop(); }
	});
}

// Java FixPath.java:145-168
float FixPath::getZ(Player& admin) {
	sendInfo(admin, "Waiting to get Z...");
	runtime::Ref<LastZ> lastZ = runtime::makeRef<LastZ>(); // parity= final ScheduledFuture<?>[] waitTask = { null };
	lastZ->value.set(admin.getZ()); // parity= float lastZ[] = { admin.getZ() };
	runtime::Ref<Player> adminRef(admin); // parity: the lambda's capture of admin
	runtime::FutureRef waitTask = utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({&admin}, [adminRef, lastZ](runtime::Future& self) { // parity= waitTask[0] = ThreadPoolManager.getInstance().scheduleAtFixedRate(() -> {
		float z = adminRef->getZ(); // parity= float z = admin.getZ();
		if (adminRef->getMoveController()->getMovementMask() == controllers::movement::MovementMask::IMMEDIATE && z != lastZ->value.get() && adminRef->isSpawned() // parity= if (admin.getMoveController().getMovementMask() == MovementMask.IMMEDIATE && z != lastZ[0] && admin.isSpawned()
			&& !adminRef->isInState(model::gameobjects::state::CreatureState::DEAD)) { // parity= && !admin.isInState(CreatureState.DEAD)) {
			self.cancel(true); // parity= waitTask[0].cancel(true);
			lastZ->value.set(z); // parity= lastZ[0] = z;
		}
	}, 500, 250);
	try {
		waitTask->get(5, runtime::TimeUnit::SECONDS); // parity= waitTask[0].get(5, TimeUnit.SECONDS);
	} catch (const runtime::TimeoutException&) { // parity= } catch (InterruptedException | ExecutionException | TimeoutException | CancellationException e) { if (e instanceof TimeoutException) {
		sendInfo(admin, "Aborted path fixing due to timeout.");
		stop();
		return 0;
	} catch (const runtime::CancellationException&) { // parity: (the multi-catch above: the other exceptions are ignored)
	} catch (const runtime::ExecutionException&) { // parity: (the same)
	} catch (const runtime::InterruptedException&) { // parity: (the same)
	}
	return lastZ->value.get(); // parity= return lastZ[0];
}

// Java FixPath.java:170-178
void FixPath::stop() {
	if (runtime::Ref<Player> runner = state().runner.get(); runner != nullptr) { // parity= if (runner != null) {
		if (runner->isOnline()) {
			if (!state().oldInvul.get() && runner->isInvulnerable()) // parity= if (!oldInvul && runner.isInvulnerable())
				runner->unsetCustomState(CustomPlayerState::INVULNERABLE);
		}
		state().runner.set(runtime::Ref<Player>()); // parity= runner = null;
	}
}

} // namespace aion::gameserver::handlers::admincommands

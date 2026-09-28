#include "aion/gameserver/spawnengine/WalkerGroup.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>
#include <string_view>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.h"
#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/spawnengine/ClusteredNpc.h"
#include "aion/gameserver/spawnengine/WalkerGroupShift.h"
#include "aion/gameserver/spawnengine/WalkerGroupType.h"

namespace aion::gameserver::spawnengine {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.spawnengine.WalkerGroup");

namespace {

using geoEngine::math::JavaFloat;
using model::templates::zone::Point2D;

/** Java: the logger of AILogger (ai/AILogger, P5-05, has no C++ header yet) */
const auto aiLog = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.ai.AILogger");

/** Java: AILogger.info(ai, message) */
void aiLoggerInfo(ai::AbstractAI& ai, std::string_view message) {
	if (ai.isLogging())
		aiLog.info("[AI] " + std::to_string(ai.getOwner().getObjectId()) + " - " + std::string(message));
}

/**
 * Java: members.stream().sorted(Comparator.comparing(ClusteredNpc::getWalkerIndex, Comparator.nullsLast(Comparator.naturalOrder())).reversed())
 * - a stable sort: null walker indexes first, then the indexes in descending order
 */
std::vector<runtime::Ptr<ClusteredNpc>> sortedMembers(const std::vector<runtime::Ptr<ClusteredNpc>>& members) {
	std::vector<runtime::Ptr<ClusteredNpc>> sorted = members;
	std::stable_sort(sorted.begin(), sorted.end(), [](const runtime::Ptr<ClusteredNpc>& a, const runtime::Ptr<ClusteredNpc>& b) {
		std::optional<int32_t> first = a->getWalkerIndex();
		std::optional<int32_t> second = b->getWalkerIndex();
		if (!first)
			return second.has_value();
		return second && *first > *second;
	});
	return sorted;
}

/** Java: members.get(0) - IndexOutOfBoundsException for an empty list */
ClusteredNpc& firstMember(const std::vector<runtime::Ptr<ClusteredNpc>>& members) {
	if (members.empty())
		throw runtime::IndexOutOfBoundsException("Index 0 out of bounds for length 0");
	return *members[0];
}

/** Java: Math.abs(float) */
float absFloat(float value) {
	return JavaFloat::mathAbs(value);
}

} // namespace

WalkerGroup::WalkerGroup(const std::vector<runtime::Ptr<ClusteredNpc>>& membersValue)
	: type(firstMember(membersValue).getWalkTemplate()->getType()), walkerXpos(firstMember(membersValue).getX()),
	  walkerYpos(firstMember(membersValue).getY()), memberSteps(runtime::Array<int32_t>::make(static_cast<int32_t>(membersValue.size()))),
	  versionId(firstMember(membersValue).getWalkTemplate()->getVersionId().value_or(std::string())) {
	// Java: members is the sorted copy; walkerXpos/walkerYpos/type/versionId come from the first element of the unsorted argument
	for (const runtime::Ptr<ClusteredNpc>& member : sortedMembers(membersValue))
		members.add(runtime::Ref<ClusteredNpc>(member));
}

WalkerGroup::~WalkerGroup() = default;

runtime::Ref<WalkerGroup> WalkerGroup::create(const std::vector<runtime::Ptr<ClusteredNpc>>& membersValue) {
	return runtime::makeRef<WalkerGroup>(membersValue);
}

void WalkerGroup::form() {
	std::vector<runtime::Ptr<ClusteredNpc>> memberList = members.snapshot();
	if (getWalkType() == WalkerGroupType::SQUARE) {
		const model::templates::walker::WalkerTemplate* walkTemplate = firstMember(memberList).getWalkTemplate();
		if (!walkTemplate->getRows()) // Java: NullPointerException in IntStream.of(null)
			throw runtime::NullPointerException("WalkerTemplate.getRows() is null for " + walkTemplate->getRouteId());
		const std::vector<int32_t>& rows = *walkTemplate->getRows();
		if (std::accumulate(rows.begin(), rows.end(), 0) != static_cast<int32_t>(memberList.size())) {
			log.warn("Invalid row sizes for walk cluster " + walkTemplate->getRouteId());
		}
		if (rows.size() == 1) {
			// Line formation: distance 2 meters from each other (divide by 2 and multiple by 2)
			// negative at left hand and positive at the right hand
			double boundsSum = 0;
			for (const runtime::Ptr<ClusteredNpc>& cNpc : memberList)
				boundsSum += cNpc->getNpc()->getObjectTemplate()->getBoundRadius()->getSide();
			float bounds = static_cast<float>(boundsSum);
			float distance = static_cast<float>(1 - static_cast<int32_t>(memberList.size())) / 2.0f * (WalkerGroupShift::DISTANCE + bounds);
			Point2D origin(walkerXpos, walkerYpos);
			Point2D destination(walkTemplate->getRouteStep(1)->getX(), walkTemplate->getRouteStep(1)->getY());
			for (size_t i = 0; i < memberList.size(); i++, distance += WalkerGroupShift::DISTANCE) {
				runtime::Ref<WalkerGroupShift> shift = WalkerGroupShift::create(distance, 0);
				Point2D loc = getLinePoint(origin, destination, *shift);
				ClusteredNpc& clusteredNpc = *memberList[i];
				clusteredNpc.set(*shift);
				clusteredNpc.setX(loc.getX());
				clusteredNpc.setY(loc.getY());
				runtime::Ptr<model::gameobjects::Npc> member = clusteredNpc.getNpc();
				member->setWalkerGroup(runtime::Ptr<WalkerGroup>(*this));
				// distance += npc.getObjectTemplate().getBoundRadius().getSide();
			}
		} else if (!rows.empty()) {
			std::vector<float> rowDistances(rows.size() - 1);
			float coronalDist = 0;
			for (size_t i = 0; i < rows.size() - 1; i++) {
				if (rows[i] % 2 != rows[i + 1] % 2)
					rowDistances[i] = 0.86602540378443864676372317075294f * WalkerGroupShift::DISTANCE;
				else
					rowDistances[i] = WalkerGroupShift::DISTANCE;
				coronalDist -= rowDistances[i];
			}
			Point2D origin(walkerXpos, walkerYpos);
			Point2D destination(walkTemplate->getRouteStep(1)->getX(), walkTemplate->getRouteStep(1)->getY());
			size_t index = 0;
			for (size_t i = 0; i < rows.size(); i++) {
				float sagittalDist = static_cast<float>(1 - rows[i]) / 2.0f * WalkerGroupShift::DISTANCE;
				for (int32_t j = 0; j < rows[i]; j++, sagittalDist += WalkerGroupShift::DISTANCE) {
					if (index > memberList.size() - 1)
						break;
					runtime::Ref<WalkerGroupShift> shift = WalkerGroupShift::create(sagittalDist, coronalDist);
					Point2D loc = getLinePoint(origin, destination, *shift);
					ClusteredNpc& cnpc = *memberList[index++];
					cnpc.set(*shift);
					cnpc.setX(loc.getX());
					cnpc.setY(loc.getY());
					cnpc.getNpc()->setWalkerGroup(runtime::Ptr<WalkerGroup>(*this));
				}
				if (i < rows.size() - 1)
					coronalDist += rowDistances[i];
			}
		}
	} else if (getWalkType() == WalkerGroupType::POINT) {
		log.warn("No formation specified for walk cluster " + firstMember(memberList).getWalkTemplate()->getRouteId());
	}
}

float WalkerGroup::getSidesExtra(std::span<const int32_t> rows, int32_t startIndex, int32_t endIndex) {
	return 0;
}

Point2D WalkerGroup::getLinePoint(const Point2D& origin, const Point2D& destination, WalkerGroupShift& shift) {
	// TODO: implement angle shift
	runtime::Ref<WalkerGroupShift> dir = getShiftSigns(origin, destination);
	std::optional<Point2D> result; // Java: Point2D result = null
	if (origin.getY() - destination.getY() == 0) {
		return Point2D(origin.getX() + dir->getCoronalShift() * shift.getCoronalShift(), origin.getY() - dir->getSagittalShift() * shift.getSagittalShift());
	} else if (origin.getX() - destination.getX() == 0) {
		return Point2D(origin.getX() + dir->getCoronalShift() * shift.getSagittalShift(), origin.getY() + dir->getCoronalShift() * shift.getCoronalShift());
	} else {
		// Java: the float division is widened to double
		double slope = static_cast<double>((origin.getX() - destination.getX()) / (origin.getY() - destination.getY()));
		double dx = static_cast<double>(absFloat(shift.getSagittalShift())) / std::sqrt(1 + slope * slope);
		if (shift.getSagittalShift() * dir->getCoronalShift() < 0)
			result = Point2D(static_cast<float>(origin.getX() - dx), static_cast<float>(origin.getY() + dx * slope));
		else
			result = Point2D(static_cast<float>(origin.getX() + dx), static_cast<float>(origin.getY() - dx * slope));
	}
	if (shift.getCoronalShift() != 0) {
		std::optional<Point2D> rotatedShift;
		if (shift.getSagittalShift() != 0) {
			runtime::Ref<WalkerGroupShift> sideShift =
				WalkerGroupShift::create(JavaFloat::signum(shift.getSagittalShift()) * absFloat(shift.getCoronalShift()), 0);
			rotatedShift = getLinePoint(origin, destination, *sideShift);
		} else {
			runtime::Ref<WalkerGroupShift> sideShift = WalkerGroupShift::create(absFloat(shift.getCoronalShift()), 0);
			rotatedShift = getLinePoint(origin, destination, *sideShift);
		}

		// since it's rotated, and perpendicular, dx and dy are reciprocal when not rotated
		float dx = absFloat(origin.getX() - rotatedShift->getX());
		float dy = absFloat(origin.getY() - rotatedShift->getY());
		if (shift.getCoronalShift() < 0) {
			if (dir->getSagittalShift() < 0 && dir->getCoronalShift() < 0) {
				result = Point2D(result->getX() + dy, result->getY() + dx);
			} else if (dir->getSagittalShift() > 0 && dir->getCoronalShift() > 0) {
				result = Point2D(result->getX() - dy, result->getY() - dx);
			} else if (dir->getSagittalShift() < 0 && dir->getCoronalShift() > 0) {
				result = Point2D(result->getX() + dy, result->getY() - dx);
			} else if (dir->getSagittalShift() > 0 && dir->getCoronalShift() < 0) {
				result = Point2D(result->getX() - dy, result->getY() + dx);
			}
		} else {
			if (dir->getSagittalShift() < 0 && dir->getCoronalShift() < 0) {
				result = Point2D(result->getX() - dy, result->getY() - dx);
			} else if (dir->getSagittalShift() > 0 && dir->getCoronalShift() > 0) {
				result = Point2D(result->getX() + dy, result->getY() + dx);
			} else if (dir->getSagittalShift() < 0 && dir->getCoronalShift() > 0) {
				result = Point2D(result->getX() - dy, result->getY() + dx);
			} else if (dir->getSagittalShift() > 0 && dir->getCoronalShift() < 0) {
				result = Point2D(result->getX() + dy, result->getY() - dx);
			}
		}
	}
	return *result;
}

runtime::Ref<WalkerGroupShift> WalkerGroup::getShiftSigns(const Point2D& origin, const Point2D& destination) {
	float dx = JavaFloat::signum(destination.getX() - origin.getX());
	float dy = JavaFloat::signum(destination.getY() - origin.getY());
	return WalkerGroupShift::create(dx, dy);
}

void WalkerGroup::setStep(model::gameobjects::Npc& member, int32_t step) {
	int32_t currentStep = 0;
	std::vector<runtime::Ptr<ClusteredNpc>> memberList = members.snapshot();
	for (size_t i = 0; i < memberList.size(); i++) {
		int32_t index = static_cast<int32_t>(i);
		if (memberSteps->get(index) > currentStep)
			currentStep = memberSteps->get(index);
		if (memberList[i]->getNpc()->equals(member)) {
			aiLoggerInfo(memberList[i]->getNpc()->getAi(), "Setting step to " + std::to_string(step));
			(*memberSteps)[index].set(step);
		}
	}
	if (step > currentStep || step == 0)
		groupStep.set(step);
}

void WalkerGroup::targetReached(ai::NpcAI& npcAI) {
	SYNCHRONIZED(members) {
		npcAI.setSubStateIfNot(ai::AISubState::WALK_WAIT_GROUP);
		bool allArrived = true;
		std::vector<runtime::Ptr<ClusteredNpc>> memberList = members.snapshot();
		for (const runtime::Ptr<ClusteredNpc>& snpc : memberList) {
			runtime::Ptr<model::gameobjects::Npc> npc = snpc->getNpc();
			allArrived &= npc->isDead() || npc->getAi().getSubState() == ai::AISubState::WALK_WAIT_GROUP;
			if (!allArrived)
				break;
		}

		for (size_t i = 0; i < memberList.size(); i++) {
			const runtime::Ptr<ClusteredNpc>& snpc = memberList[i];
			if (!snpc->getNpc()->isDead() && snpc->getNpc()->getAi().getSubState() == ai::AISubState::WALK_WAIT_GROUP) {
				if (memberSteps->get(static_cast<int32_t>(i)) == groupStep.get() && !allArrived)
					snpc->getNpc()->getMoveController()->abortMove();
				else
					// Java WalkerGroup.java:218. The cast is Java's own; A-00's DummyNpcAI keeps the invariant it relies on for an npc whose AI
					// handler is not registered (m5b-plan.md D15).
					ai::manager::WalkManager::targetReached(*runtime::cast<ai::NpcAI>(snpc->getNpc()->getAi()));
			}
		}
	}
}

void WalkerGroup::spawn() {
	for (runtime::Ptr<ClusteredNpc> snpc : members.snapshot()) {
		float height = getHeight(snpc->getX(), snpc->getY(), *snpc->getNpc()->getSpawn());
		snpc->spawn(height);
	}
	isSpawned_.set(true);
}

void WalkerGroup::respawn(model::gameobjects::Npc& npc) {
	std::vector<runtime::Ptr<ClusteredNpc>> memberList = members.snapshot();
	for (size_t index = 0; index < memberList.size(); index++) {
		const runtime::Ptr<ClusteredNpc>& snpc = memberList[index];
		std::optional<int32_t> walkerIndex = npc.getSpawn()->getWalkerIndex();
		// Java compares the Integer walker indexes with == (identity; equal for the small cached values the data uses)
		if (snpc->getNpc()->getNpcId() == npc.getNpcId() &&
			((!walkerIndex && snpc->getNpc()->isDead()) || (walkerIndex && walkerIndex == snpc->getWalkerIndex()))) {
			SYNCHRONIZED(members) {
				int32_t i = static_cast<int32_t>(index);
				(*memberSteps)[i].set(std::max(0, groupStep.get() - 1));
				const model::templates::walker::RouteStep* step = snpc->getWalkTemplate()->getRouteStep(memberSteps->get(i));
				npc.getMoveController()->setWalkerTemplate(snpc->getWalkTemplate(), memberSteps->get(i));
				snpc->setNpc(npc, step);
				snpc->spawn(step->getZ());
			}
			break;
		}
	}
}

void WalkerGroup::despawn() {
	for (runtime::Ptr<ClusteredNpc> snpc : members.snapshot()) {
		snpc->despawn();
		// reset positions
		form();
		for (int32_t index = 0; index < memberSteps->length(); index++)
			(*memberSteps)[index].set(0);
		groupStep.set(0);
	}
	isSpawned_.set(false);
}

runtime::Ptr<ClusteredNpc> WalkerGroup::getClusterData(model::gameobjects::Npc& npc) {
	for (runtime::Ptr<ClusteredNpc> snpc : members.snapshot()) {
		if (snpc->getNpc()->equals(npc))
			return snpc;
	}
	return nullptr;
}

float WalkerGroup::getHeight(float x, float y, model::templates::spawns::SpawnTemplate& template_) {
	/*
	 * if (GeoService.getInstance().isGeoOn()) { return GeoService.getInstance().getZ(template.getWorldId(), x, y, z, ); }
	 */
	return template_.getZ();
}

int32_t WalkerGroup::getPool() {
	return members.size();
}

bool WalkerGroup::isLinearlyPositioned(model::gameobjects::Npc& npc) {
	if (type != WalkerGroupType::SQUARE)
		return false;
	for (runtime::Ptr<ClusteredNpc> snpc : members.snapshot()) {
		if (snpc->getNpc()->equals(npc)) {
			const auto& rows = snpc->getWalkTemplate()->getRows();
			if (!rows) // Java: NullPointerException on getRows().length
				throw runtime::NullPointerException("WalkerTemplate.getRows() is null for " + snpc->getWalkTemplate()->getRouteId());
			return rows->size() == 1;
		}
	}
	return false;
}

} // namespace aion::gameserver::spawnengine

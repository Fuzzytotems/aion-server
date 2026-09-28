#pragma once

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"

namespace aion::gameserver::model::team {

/**
 * C++: Java `GeneralTeam<M extends AionObject, TM extends TeamMember<M>>` is one non-template class (docs/design/hub-headers.md §8.1): M is
 * spelled AionObject, TM TeamMember. The ReentrantLock teamLock is a Monitor. Written with the S0b objects group because TemporaryPlayerTeam
 * (a hub) derives it.
 *
 * @author ATracer
 */
class GeneralTeam : public gameobjects::AionObject {
	AION_MAKE_REF_FRIEND
private:
	// Java: private final static Logger log = LoggerFactory.getLogger(GeneralTeam.class) - namespace-scope logger in GeneralTeam.cpp

protected:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<TeamMember>> members{AION_LOCK_CLASS(GeneralTeam::members#stripe)};
	runtime::Monitor teamLock{AION_LOCK_CLASS(GeneralTeam::teamLock)};

private:
	runtime::Field<runtime::Ref<TeamMember>> leader{};

protected:
	GeneralTeam(int32_t objId, bool autoReleaseObjectId);
	~GeneralTeam() override;

public:
	/** Java final */
	void onEvent(TeamEvent& event);

	/** Java final */
	runtime::Ptr<TeamMember> getMember(int32_t objectId);

	/** Java final */
	bool hasMember(int32_t objectId);

	virtual void addMember(TeamMember& member);

	/** Java final */
	runtime::Ptr<TeamMember> removeMember(TeamMember& member);

	/** Java final */
	runtime::Ptr<TeamMember> removeMember(int32_t objectId);

protected:
	virtual void onRemoveMember(TeamMember& member) = 0;

public:
	/**
	 * Apply some function on all team members<br>
	 * Should be used only to change state of the group or its members
	 */
	void forEachTeamMember(const std::function<void(TeamMember&)>& consumer);

	/**
	 * Apply some function on all team member's objects<br>
	 * Should be used only to change state of the group or its members
	 */
	void forEach(const std::function<void(gameobjects::AionObject&)>& consumer);

	/**
	 * Apply some function on all team member's objects, until the function returns false<br>
	 * Should be used only to change state of the group or its members
	 */
	void applyOnMembers(const std::function<bool(gameobjects::AionObject&)>& function);

	std::vector<runtime::Ptr<TeamMember>> filter(const std::function<bool(TeamMember&)>& predicate);

	/** Java return type List<M> */
	std::vector<runtime::Ptr<gameobjects::AionObject>> filterMembers(const std::function<bool(gameobjects::AionObject&)>& predicate);

	/** Java return type List<M> */
	std::vector<runtime::Ptr<gameobjects::AionObject>> getMembers();

	int32_t size();

	/** Java final */
	bool isDisbanded();

	/** Java final */
	bool shouldDisband();

	/** Java final */
	bool isFull();

	/** Java final */
	int32_t getTeamId();

	std::string getName() override;

	/** Java final */
	runtime::Ptr<TeamMember> getLeader() const { return leader.get(); }

	/** Java final; return type M */
	runtime::Ptr<gameobjects::AionObject> getLeaderObject();

	/** Java final */
	bool isLeader(gameobjects::AionObject& member);

	/** Java final */
	void changeLeader(TeamMember& member);

protected:
	/** Java final */
	void setLeader(TeamMember& member);

	/** Java final */
	void lock();

	/** Java final */
	void unlock();

public:
	virtual Race getRace() = 0;

	virtual int32_t getMaxMemberCount() = 0;

	virtual std::vector<runtime::Ptr<gameobjects::player::Player>> getOnlineMembers() = 0;

	virtual runtime::Ptr<common::legacy::LootGroupRules> getLootGroupRules() = 0;

	virtual void sendPackets(std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>> packets) = 0;

	/** Java Predicate<M> */
	virtual void sendPacket(const std::function<bool(gameobjects::AionObject&)>& predicate,
		std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>> packets) = 0;
};

} // namespace aion::gameserver::model::team

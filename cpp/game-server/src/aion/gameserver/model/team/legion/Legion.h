#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/legion/LegionHistoryAction_Type.h"
#include "aion/gameserver/model/team/legion/fwd.h"

namespace aion::gameserver::model::team::legion {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (through AionObject, fieldmap K4 `LegionMember.legion`), created with
 * create(). The legion warehouse is a part created by the constructor (`const std::unique_ptr<LegionWarehouse>`). The history map is keyed by
 * `LegionHistoryAction.Type`, which is nested in an enum: `LegionHistoryAction_Type` (hub-headers.md §13). The member id list is replaced by
 * setMemberIds (`Field<Ref<RcCopyOnWriteArrayList>>`); Java stores the caller's list, the C++ setter copies the ids into a new list.
 * The constructor ends with `setHistory(Collections.emptyMap())`, whose body is unported, so it stays `AION_UNPORTED` at that statement.
 *
 * @author Simple
 */
class Legion : public gameobjects::AionObject {
	AION_MAKE_REF_FRIEND
public:
	/** Java record Announcement(String message, Timestamp time) */
	class Announcement : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	private:
		const std::string message_;
		const commons::database::Timestamp time_;

	protected:
		Announcement(std::string_view message, commons::database::Timestamp time);
		~Announcement() override;

	public:
		/** Java: new Announcement(message, time) (canonical record constructor) */
		static runtime::Ref<Announcement> create(std::string_view message, commons::database::Timestamp time);

		std::string message() const { return message_; }

		commons::database::Timestamp time() const { return time_; }

		/** Java record equals (all components) */
		bool equals(const Announcement& obj) const;

		/** Java record hashCode */
		int32_t hashCode() const;
	};

private:
	runtime::Field<std::string> legionName{};
	runtime::Field<int32_t> legionLevel{1};
	runtime::Field<int64_t> contributionPoints{0};
	runtime::Field<runtime::Ref<runtime::RcCopyOnWriteArrayList<int32_t>>> memberIds; // Java: = new CopyOnWriteArrayList<>() (constructor)
	runtime::Field<int16_t> deputyPermission{0x1E0C};
	runtime::Field<int16_t> centurionPermission{0x1C08};
	runtime::Field<int16_t> legionaryPermission{0x1800};
	runtime::Field<int16_t> volunteerPermission{0x800};
	runtime::Field<int32_t> disbandTime{};
	runtime::Field<runtime::Ref<Legion::Announcement>> announcement{};
	runtime::Field<runtime::Ref<LegionEmblem>> legionEmblem; // Java: = new LegionEmblem() (constructor)
	const std::unique_ptr<LegionWarehouse> legionWarehouse;
	// fieldmap: LegionHistoryAction.Type is nested in an enum, spelled LegionHistoryAction_Type (hub-headers.md §13)
	runtime::EnumMap<LegionHistoryAction_Type, runtime::Ref<runtime::RcArrayList<runtime::Ref<LegionHistoryEntry>>>> legionHistoryByType{
		AION_LOCK_CLASS(Legion::legionHistoryByType)};
	runtime::AtomicBoolean hasBonus_{AION_LOCK_CLASS(Legion::hasBonus), false};
	runtime::Field<int32_t> occupiedLegionDominion{0};
	runtime::Field<int32_t> currentLegionDominion{0};
	runtime::Field<int32_t> lastLegionDominion{0};

protected:
	/** Only called when a legion is created! */
	Legion(int32_t legionId, std::string_view legionName);
	~Legion() override;

public:
	/** Java: new Legion(legionId, legionName) */
	static runtime::Ref<Legion> create(int32_t legionId, std::string_view legionName);

	/** @return legion id */
	int32_t getLegionId() const { return getObjectId(); }

	/** @return legion name */
	std::string getName() override { return legionName.get(); }

	/** @param value the legionName to set */
	void setName(std::string_view value) { legionName.set(std::string(value)); }

	void setMemberIds(const std::vector<int32_t>& memberIds);

	/** @return the live member id list */
	runtime::Ptr<runtime::RcCopyOnWriteArrayList<int32_t>> getMemberIds() const { return memberIds.get(); }

	std::vector<runtime::Ptr<LegionMember>> getMembers();

private:
	/** Java Stream<LegionMember> (§7.2) */
	std::vector<runtime::Ptr<LegionMember>> streamMembers();

public:
	std::vector<runtime::Ptr<gameobjects::player::Player>> getOnlinePlayers();

	/** @throws NoSuchElementException if the legion has no brigade general (Java orElseThrow) */
	runtime::Ptr<LegionMember> getBrigadeGeneral();

	/** Add a legionMember to the legionMembers list */
	bool addLegionMember(int32_t playerObjId);

	/** Removes a legionMember from the legionMembers list */
	void removeMember(int32_t playerObjId);

	/** This method will set the permissions */
	void setLegionPermissions(int16_t deputyPermission, int16_t centurionPermission, int16_t legionaryPermission, int16_t volunteerPermission);

	int16_t getDeputyPermission() const { return deputyPermission.get(); }

	int16_t getCenturionPermission() const { return centurionPermission.get(); }

	int16_t getLegionaryPermission() const { return legionaryPermission.get(); }

	int16_t getVolunteerPermission() const { return volunteerPermission.get(); }

	/** @return the legionLevel */
	int32_t getLegionLevel() const { return legionLevel.get(); }

	/** @param legionLevel the legionLevel to set */
	void setLegionLevel(int32_t legionLevel);

	/** @param contributionPoints the contributionPoints to add */
	void addContributionPoints(int64_t contributionPoints);

	/** @param value the contributionPoints to set */
	void setContributionPoints(int64_t value) { contributionPoints.set(value); }

	/** @return the contributionPoints */
	int64_t getContributionPoints() const { return contributionPoints.get(); }

	/** This method will check whether a legion has enough members to level up */
	bool hasRequiredMembers();

	/** This method will return the kinah price required to level up */
	int32_t getKinahPrice();

	/** This method will return the contribution points required to level up */
	int32_t getContributionPrice();

private:
	/** This method will return true if a legion is able to add a member */
	bool canAddMember();

public:
	/** @return the announcement, null if there is none */
	runtime::Ptr<Legion::Announcement> getAnnouncement() const { return announcement.get(); }

	void setAnnouncement(runtime::Ptr<Legion::Announcement> announcement);

	/** @param value the disbandTime to set */
	void setDisbandTime(int32_t value) { disbandTime.set(value); }

	/** @return the disbandTime */
	int32_t getDisbandTime() const { return disbandTime.get(); }

	/** @return true if currently disbanding */
	bool isDisbanding();

	/** This function checks if object id is in list */
	bool isMember(int32_t playerObjId);

	/** @param legionEmblem the legionEmblem to set */
	void setLegionEmblem(runtime::Ptr<LegionEmblem> legionEmblem);

	/** @return the legionEmblem */
	runtime::Ptr<LegionEmblem> getLegionEmblem() const { return legionEmblem.get(); }

	/** @return the legionWarehouse */
	LegionWarehouse& getLegionWarehouse() const;

	int32_t getWarehouseExpansions();

	/** @return a copy of the history list of the type */
	std::vector<runtime::Ptr<LegionHistoryEntry>> getHistory(LegionHistoryAction_Type type);

	/**
	 * Adds the history entry at the top of the list and removes entries older than a year (except the ones on the first page)
	 *
	 * @return the removed entries (held by the returned Refs: nothing else keeps them)
	 */
	std::vector<runtime::Ref<LegionHistoryEntry>> addHistory(LegionHistoryEntry& entry);

	/** Replaces the history (the lists are stored) */
	void setHistory(const std::map<LegionHistoryAction_Type, std::vector<runtime::Ref<LegionHistoryEntry>>>& history);

	void addBonus();

	void removeBonus();

	bool hasBonus();

	int32_t getOccupiedLegionDominion() const { return occupiedLegionDominion.get(); }

	int32_t getCurrentLegionDominion() const { return currentLegionDominion.get(); }

	int32_t getLastLegionDominion() const { return lastLegionDominion.get(); }

	void setOccupiedLegionDominion(int32_t value) { occupiedLegionDominion.set(value); }

	void setCurrentLegionDominion(int32_t value) { currentLegionDominion.set(value); }

	void setLastLegionDominion(int32_t value) { lastLegionDominion.set(value); }

	std::string toString() override;
};

} // namespace aion::gameserver::model::team::legion

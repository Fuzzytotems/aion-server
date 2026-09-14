#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/storage/fwd.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * This class is designed to do all the work related with loading/storing legions and their members.
 * <p>
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * Legion.h is included for the nested record Legion::Announcement (hub-headers.md §9.3); the inner class LegionRestrictions has no owner
 * member (it keeps no state).
 *
 * @author Simple, cura, Source, Neon
 */
class LegionService : public runtime::Immortal {
private:
	/**
	 * This class contains all restrictions for legion features
	 *
	 * @author Simple
	 */
	class LegionRestrictions : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	protected:
		LegionRestrictions() = default;

	public:
		/** C++ only (hub-headers.md §10.1): Java `new LegionRestrictions()` (an inner class without state; bodies use LegionService::getInstance()) */
		static runtime::Ref<LegionRestrictions> create();
		static constexpr int32_t MIN_EMBLEM_ID = 0;
		static constexpr int32_t MAX_EMBLEM_ID = 49;
		bool canCreateLegion(model::gameobjects::player::Player& activePlayer, std::string_view legionName);
		bool canInvitePlayer(model::gameobjects::player::Player& activePlayer, runtime::Ptr<model::gameobjects::player::Player> targetPlayer);
		bool canKickPlayer(model::gameobjects::player::Player& player, std::string_view charName, runtime::Ptr<model::team::legion::LegionMember> legionMember);
		bool canAppointBrigadeGeneral(model::gameobjects::player::Player& activePlayer, model::gameobjects::player::Player& targetPlayer);
		bool canAppointRank(model::gameobjects::player::Player& activePlayer, runtime::Ptr<model::team::legion::LegionMember> targetMember);
		bool canChangeSelfIntro(model::gameobjects::player::Player& activePlayer, std::string_view newSelfIntro);
		bool canChangeLevel(model::gameobjects::player::Player& activePlayer);
		bool canChangeNickname(model::gameobjects::player::Player& player, runtime::Ptr<model::team::legion::LegionMember> member, std::string_view memberName, std::string_view newNickname);
		bool canDisbandLegion(model::gameobjects::player::Player& activePlayer);
		bool canLeave(model::gameobjects::player::Player& activePlayer);
		bool canRecreateLegion(model::gameobjects::player::Player& activePlayer);
		bool canUploadEmblem(model::gameobjects::player::Player& activePlayer, bool initUpload);
		bool canOpenWarehouse(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc);
		bool canStoreLegionEmblem(model::gameobjects::player::Player& activePlayer, int32_t emblemId);
		bool isBrigadeGeneral(model::gameobjects::player::Player& player);
		bool isFreeName(std::string_view name);
		bool isValidSelfIntro(std::string_view name);
		bool isValidNickname(std::string_view name);
	protected:
		~LegionRestrictions() override;
	};
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::team::legion::Legion>> legionsById{AION_LOCK_CLASS(LegionService::legionsById#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::team::legion::LegionMember>> legionMemberById{AION_LOCK_CLASS(LegionService::legionMemberById#stripe)}; // Java: = new ConcurrentHashMap<>()
	static constexpr int32_t MAX_LEGION_LEVEL = 8;
	const runtime::Ref<LegionService::LegionRestrictions> legionRestrictions; // Java: = new LegionRestrictions()
public:
	static LegionService& getInstance(); // Java singleton
private:
	LegionService();
	~LegionService();
	void storeLegion(model::team::legion::Legion& legion, bool newLegion);
	void storeLegion(model::team::legion::Legion& legion);
public:
	void storeLegionMember(model::team::legion::LegionMember& legionMember);
	std::vector<runtime::Ptr<model::team::legion::Legion>> getCachedLegions();
private:
	void addCachedLegion(model::team::legion::Legion& legion);
public:
	static void deleteLegionFromDB(int32_t legionId);
private:
	/** This method will remove the legion member from cache and the database */
	void deleteLegionMemberFromDB(model::team::legion::LegionMember& legionMember);
public:
	runtime::Ptr<model::team::legion::Legion> getLegion(std::string_view legionName);
	runtime::Ptr<model::team::legion::Legion> getLegion(int32_t legionId);
private:
	void loadLegionInfo(model::team::legion::Legion& legion);
	runtime::Ptr<model::team::legion::LegionMember> getLegionMember(std::string_view name);
public:
	runtime::Ptr<model::team::legion::LegionMember> getLegionMember(int32_t playerObjId);
	runtime::Ptr<model::team::legion::LegionMember> getLegionMember(model::gameobjects::player::PlayerCommonData& playerCommonData);
private:
	runtime::Ptr<model::team::legion::LegionMember> getLegionMember(int32_t playerObjectId, runtime::Ptr<model::gameobjects::player::PlayerCommonData> playerCommonData);
	/** Method that checks if a legion is disbanding */
	bool checkDisband(model::team::legion::Legion& legion);
public:
	/** This method will disband a legion and update all members */
	void disbandLegion(model::team::legion::Legion& legion);
	void requestDisbandLegion(model::gameobjects::Npc& npc, model::gameobjects::player::Player& activePlayer);
	void createLegion(model::gameobjects::player::Player& activePlayer, std::string_view legionName);
	bool addToLegion(model::team::legion::Legion& legion, model::gameobjects::player::Player& invited, model::gameobjects::player::Player& inviter);
	void invitePlayerToLegion(model::gameobjects::player::Player& activePlayer, std::string_view targetName);
private:
	/** Displays current legion announcement */
	void displayLegionAnnouncement(model::gameobjects::player::Player& targetPlayer, runtime::Ptr<model::team::legion::Legion::Announcement> announcement);
public:
	void startBrigadeGeneralChangeProcess(model::gameobjects::player::Player& legionLeader, std::string_view memberName);
private:
	void appointBrigadeGeneral(model::gameobjects::player::Player& activePlayer, model::gameobjects::player::Player& targetPlayer);
public:
	void appointBrigadeGeneral(model::team::legion::LegionMember& member);
	/** This method will handle the process when a member is demoted or promoted. */
	void appointRank(model::gameobjects::player::Player& player, std::string_view charName, int32_t rankId);
	void changeSelfIntro(model::gameobjects::player::Player& activePlayer, std::string_view newSelfIntro);
	void changePermissions(model::gameobjects::player::Player& player, int16_t deputyPermission, int16_t centurionPermission, int16_t legionarPermission, int16_t volunteerPermission);
	/** This method will handle the leveling up of a legion */
	void requestChangeLevel(model::gameobjects::player::Player& activePlayer);
	/** This method will change the legion level and send update to online members */
	void changeLevel(model::team::legion::Legion& legion, int32_t newLevel, bool save);
	void changeNickname(model::gameobjects::player::Player& activePlayer, std::string_view memberName, std::string_view newNickname);
private:
	/** This method will remove legion from all legion members online after a legion has been disbanded */
	void updateAfterDisbandLegion(model::team::legion::Legion& legion);
	void updateMembersEmblem(model::team::legion::Legion& legion);
	/** This method will send a packet to every legion member and update them about the disbanding */
	void updateMembersOfDisbandLegion(model::team::legion::Legion& legion, int32_t unixTime);
	/** This method will send a packet to every legion member and update them about the recreation */
	void updateMembersOfRecreateLegion(model::team::legion::Legion& legion);
public:
	void storeLegionEmblem(model::gameobjects::player::Player& activePlayer, int32_t emblemId, int32_t color_a, int32_t color_r, int32_t color_g, int32_t color_b, model::team::legion::LegionEmblemType emblemType);
	void openLegionWarehouse(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc);
	void recreateLegion(model::gameobjects::Npc& npc, model::gameobjects::player::Player& activePlayer);
	void LegionWhUpdate(model::gameobjects::player::Player& player);
	/** This method will update all players about the level/class/map/online change */
	void updateMemberInfo(model::gameobjects::player::Player& player);
	/** This method will set the contribution points, specially for legion command */
	void setContributionPoints(model::team::legion::Legion& legion, int64_t newPoints, bool save);
	void uploadEmblemInfo(model::gameobjects::player::Player& activePlayer, int32_t totalSize, int32_t color_a, int32_t color_r, int32_t color_g, int32_t color_b, model::team::legion::LegionEmblemType emblemType);
	void uploadEmblemData(model::gameobjects::player::Player& activePlayer, int32_t size, std::span<const uint8_t> data);
	void sendEmblemData(model::gameobjects::player::Player& player, model::team::legion::LegionEmblem& legionEmblem, int32_t legionId, std::string_view legionName);
	/** This will add a new announcement to the DB and change the current announcement */
	void changeAnnouncement(model::gameobjects::player::Player& activePlayer, std::string_view message);
private:
	void addHistory(model::team::legion::Legion& legion, std::string_view text, model::team::legion::LegionHistoryAction action);
public:
	void addRewardHistory(model::team::legion::Legion& legion, int64_t kinahAmount, model::team::legion::LegionHistoryAction action, int32_t fortressId);
	/** This method will add a new history for a legion */
	void addHistory(model::team::legion::Legion& legion, std::string_view name, model::team::legion::LegionHistoryAction action, std::string_view description);
private:
	/** This method will add a new legion member to a legion with VOLUNTEER rank */
	void addLegionMember(model::team::legion::Legion& legion, model::gameobjects::player::Player& player);
	void addLegionMember(model::team::legion::Legion& legion, model::gameobjects::player::Player& player, model::team::legion::LegionRank rank);
	bool removeLegionMember(model::gameobjects::player::Player& player);
	/** @param kickerName null when the member leaves by itself (removeLegionMember(player) passes null; the message id depends on it) */
	bool removeLegionMember(runtime::Ptr<model::team::legion::LegionMember> legionMember, std::optional<std::string_view> kickerName);
public:
	void kickMember(model::gameobjects::player::Player& player, std::string_view memberName);
	bool leaveLegion(model::gameobjects::player::Player& player, bool skipChecks);
	void onLogin(model::gameobjects::player::Player& activePlayer);
	void onLogout(model::gameobjects::player::Player& player);
	void addWHItemHistory(model::gameobjects::player::Player& player, int32_t itemId, int64_t count, model::items::storage::IStorage& sourceStorage, model::items::storage::IStorage& destStorage);
	void updateLegionMemberList(model::gameobjects::player::Player& player, bool broadcastToLegion);
	void updateLegionMemberList(runtime::Ptr<model::gameobjects::player::Player> player, bool broadcastToLegion, std::optional<int32_t> excludedPlayerId);
	bool tryRename(model::team::legion::Legion& legion, std::string_view name, model::gameobjects::player::Player& player, std::optional<int32_t> legionNameChangeTicketItemObjId);
	void joinLegionDominion(model::gameobjects::player::Player& player, int32_t locId);
};

} // namespace aion::gameserver::services

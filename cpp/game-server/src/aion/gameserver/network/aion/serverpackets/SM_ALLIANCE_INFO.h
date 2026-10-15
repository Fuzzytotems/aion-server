#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sarynth, xTz
 */
class SM_ALLIANCE_INFO : public AionServerPacket {
private:
	class AllianceInfo : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	public:
		runtime::Field<int32_t> alliancePosition{};
		runtime::Field<int32_t> allianceObjectId{};
		runtime::Field<int32_t> memberCount{};
		runtime::Field<std::string> captainName{};
		runtime::Field<int32_t> captainWorldId{0};
		int32_t getAlliancePosition() const { return this->alliancePosition.get(); }
		void setAlliancePosition(int32_t value) { this->alliancePosition.set(value); }
		int32_t getAllianceObjectId() const { return this->allianceObjectId.get(); }
		void setAllianceObjectId(int32_t value) { this->allianceObjectId.set(value); }
		void setMemberCount(int32_t value) { this->memberCount.set(value); }
		int32_t getMemberCount() const { return this->memberCount.get(); }
		std::string getCaptainName() const { return this->captainName.get(); }
		void setCaptainName(std::string_view value) { this->captainName.set(std::string(value)); }
		int32_t getCaptainWorldId() const { return this->captainWorldId.get(); }
		void setCaptainWorldId(int32_t value) { this->captainWorldId.set(value); }
		static runtime::Ref<AllianceInfo> create();

	protected:
		AllianceInfo();
		~AllianceInfo() override;
	};
	runtime::Ref<model::team::common::legacy::LootGroupRules> lootRules{};
	runtime::Ref<model::team::common::legacy::LootGroupRules> lootLeagueRules{};
	runtime::Ref<model::team::alliance::PlayerAlliance> alliance{};
	int32_t leaderid{};
	int32_t groupid{};
	int32_t type{};
	int32_t subType{};
	int32_t messageId{};
	std::string message{};
	int32_t leagueId{};
	std::vector<runtime::Ref<SM_ALLIANCE_INFO::AllianceInfo>> leagueData{}; // Java: = new ArrayList<>()

public:
	static constexpr int32_t VICECAPTAIN_PROMOTE = 1300984;
	static constexpr int32_t VICECAPTAIN_DEMOTE = 1300985;
	static constexpr int32_t LEAGUE_ALLIANCE_ENTERED = 1400560;
	static constexpr int32_t LEAGUE_JOINED_ALLIANCE = 1400561;
	static constexpr int32_t LEAGUE_LEFT_ME = 1400571;
	static constexpr int32_t LEAGUE_LEFT_HIM = 1400572;
	static constexpr int32_t LEAGUE_EXPEL = 1400574;
	static constexpr int32_t LEAGUE_EXPELLED = 1400576;
	static constexpr int32_t LEAGUE_DISPERSED = 1400579;
	explicit SM_ALLIANCE_INFO(model::team::alliance::PlayerAlliance& alliance);
	SM_ALLIANCE_INFO(model::team::alliance::PlayerAlliance& alliance, model::team::alliance::PlayerAlliance& skipped);
	SM_ALLIANCE_INFO(model::team::alliance::PlayerAlliance& alliance, int32_t messageId, std::string_view message);
	SM_ALLIANCE_INFO(model::team::alliance::PlayerAlliance& alliance, int32_t messageId, std::string_view message,
		runtime::Ptr<model::team::alliance::PlayerAlliance> skipped);
	~SM_ALLIANCE_INFO() override;

	/** writeImpl reads the connection: serialized per recipient (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets

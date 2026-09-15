#include "aion/gameserver/network/aion/serverpackets/SM_VERSION_CHECK.h"

#include <chrono>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/commons/utils/info/SystemInfo.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/model/EventThemeInfo.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/network/chatserver/ChatServer.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/**
 * Java: GameServer.getRatiosFor(race). GameServer.h is a file of P5-14 that does not exist yet (header request network-1 was rejected for the same
 * reason); the ratios are only changed by GameServer.updateRatio, whose callers (PlayerController) reach the unported stand-in, so they are 0 like
 * Java's initial values. TODO(P5-14): call GameServer::getRatiosFor.
 */
float gameServerRatiosFor(model::Race) {
	return 0.0f;
}

/** Java: GameServer.getCountFor(race), 0 until P5-14 (see gameServerRatiosFor). TODO(P5-14): call GameServer::getCountFor. */
int32_t gameServerCountFor(model::Race) {
	return 0;
}

/** Java: GameServer.START_TIME_SECONDS = (int) (ManagementFactory.getRuntimeMXBean().getStartTime() / 1000). TODO(P5-14): GameServer's constant */
int32_t startTimeSeconds() {
	const auto startTime = commons::utils::info::SystemInfo::getProcessStartTime();
	return static_cast<int32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(startTime.time_since_epoch()).count() / 1000);
}

} // namespace

SM_VERSION_CHECK::SM_VERSION_CHECK(model::EventTheme cityDecorationValue)
	: SM_VERSION_CHECK(INTERNAL_VERSION, cityDecorationValue) {
}

SM_VERSION_CHECK::SM_VERSION_CHECK(int32_t versionValue, model::EventTheme cityDecorationValue)
	: AionServerPacket(opcodeOf<SM_VERSION_CHECK>), version(versionValue), cityDecoration(cityDecorationValue) {
}

void SM_VERSION_CHECK::writeImpl(AionConnection* con) {
	using configs::main::GSConfig;
	using configs::main::MembershipConfig;
	int32_t characterLimitCount = GSConfig::CHARACTER_LIMIT_COUNT.load();
	int32_t limitFactionMode = GSConfig::CHARACTER_FACTION_LIMITATION_MODE.load();
	if (MembershipConfig::CHARACTER_ADDITIONAL_COUNT.load() > characterLimitCount && MembershipConfig::CHARACTER_ADDITIONAL_ENABLE.load() != 10)
		characterLimitCount = MembershipConfig::CHARACTER_ADDITIONAL_COUNT.load();
	if (GSConfig::ENABLE_RATIO_LIMITATION.load()) {
		if (gameServerRatiosFor(model::Race::ELYOS) > static_cast<float>(GSConfig::RATIO_MIN_VALUE.load()))
			limitFactionMode = 1;
		else if (gameServerRatiosFor(model::Race::ASMODIANS) > static_cast<float>(GSConfig::RATIO_MIN_VALUE.load()))
			limitFactionMode = 2;
		else if (gameServerCountFor(model::Race::ELYOS) + gameServerCountFor(model::Race::ASMODIANS) > GSConfig::RATIO_HIGH_PLAYER_COUNT_DISABLING.load())
			limitFactionMode = 3;
	}
	if (version != INTERNAL_VERSION) {
		writeC(1); // answerID
		// 0 - ok (no message)
		// 1 - The client version is not compatible with the game server.
		// 2 - The NPC script version is not compatible with the game server.
		// 3, 4, ... - An unknown error has occurred while checking the game server version.
		return;
	}
	writeC(0); // answerID
	writeC(configs::network::NetworkConfig::GAMESERVER_ID.load()); // serverId
	writeD(150602); // GSServBuildDate (year month day)
	writeD(150326); // DBServBuildDate (year month day)
	writeD(0x00); // 0
	writeD(150317); // NPCServBuildDate (year month day)
	writeD(startTimeSeconds()); // start server time in seconds
	writeC(0x00); // 0
	writeC(GSConfig::SERVER_COUNTRY_CODE.load()); // country code
	writeC(0x00); // 0
	writeC((characterLimitCount * network::loginserver::LoginServer::getInstance().getGameServerCount() * 0x10) | (limitFactionMode * 4)
		| GSConfig::CHARACTER_CREATION_MODE.load()); // ServerFlag
	writeD(static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000)); // PacketGenTimeOnServ (current UTC time in seconds)
	writeH(GSConfig::MIN_SKILL_CAST_INTERVAL_MILLIS.load()); // skillPacketDelay
	writeC(1); // enableClientPet (always 1)
	writeC(10); // minSendMailLevel (now 5 on official)
	writeC(1); // minReceiveWhisperLevel (now 15 on official)
	writeC(10); // minReceiveMailLevel
	writeC(GSConfig::CHAT_SERVER_MIN_LEVEL.load()); // ChannelChatLevel (min level to write in channel chats)
	writeC(20); // Trial_ChannelChatLevel (now 1 on official)
	writeC(20); // Trial_Channelchatwritelevel1 (before 30, now 66 on official)
	writeC(1); // Trial_Channelchatwritelevel2 (always 1)
	writeH(2); // MatchingCoolTimeSEC (always 2)
	writeC(GSConfig::CHARACTER_REENTRY_TIME.load());
	writeD(model::getId(cityDecoration)); // SceneStatus
	writeC(0); // fatigueKoreaUse
	writeD(-utils::time::ServerTime::getStandardOffset()); // server time zone offset relative to UTC in seconds (excluding daylight savings)
	writeC(0x04); // MaxHousingChargePerid
	writeD(40014200); // spawn_version (4.0)
	writeC(1); // DisposableItemTrade
	writeD(0); // EnableNeutralChat (4.0)
	writeD(0); // updateServerAddr (4.5)
	writeH(3000); // updateServerPort (4.5)
	writeH(1); // updateServerVersionCheckType (4.5)
	writeC(0); // ReduceSellPriceforGold (4.7)
	writeC(1); // RestrictWareandChargebyRank (4.7)
	writeD(-utils::time::ServerTime::getDaylightSavings()); // TimeDstBias (servers current daylight saving time offset in seconds)
	writeC(1); // SetDefaultAnimLength (4.7)
	writeC(1); // 1 = activate stonespear siege (4.8)
	writeD(0); // 1 = activate master server (4.8)
	writeC(0); // unk/not used? (4.8)
	writeC(detail::isAtreianPassportDisabled() ? 1 : 0); // remove Atreian Passport menu entry 0/1 (4.8)
	writeC(0); // Newbie or comeback icons/etc disable/enable? 0/1 (4.8)
	writeC(0); // Disable evolution? 0/1 (4.8)
	writeC(0); // Item destroy on Enchant? 0/1 (4.8)
	writeC(0); // Disable some item wear level check? 0/1 (4.8)
	writeC(0); // Item related enable/disable? 0/1 (4.8)
	writeC(0); // Special page(rank points?) enable/disable? / CreateAllPlayerClass? 0/1 (4.8) // TODO test
	writeD(GSConfig::ITEM_WRAP_LIMIT.load()); // incFreeTradePackCount (4.8)
	writeD(1000); // decDropProb (rate modifier, divide by 1000) not used? (4.8)
	writeD(1000); // decUserSellItemPrice (rate modifier, divide by 1000) (4.8)
	writeD(1000); // decUserSellAPPrice (rate modifier, divide by 1000) (4.8)
	writeD(1000); // incNpcSellItemMedalPrice (rate modifier, divide by 1000) (4.8)
	writeD(1000); // incNpcSellItemCoinPrice (rate modifier, divide by 1000) (4.8)
	writeD(1000); // incNpcSellItemAPPrice (rate modifier, divide by 1000) (4.8)
	writeD(1000); // incNpcSellItemQinaPrice (rate modifier, divide by 1000) (4.8)
	writeD(1000); // incNpcSellItemAPQinaPrice (rate modifier, divide by 1000) (4.8)
	writeD(1000); // incItemUpgradeItemCnt (rate modifier, divide by 1000) (4.8)
	writeD(1000); // incItemUpgradeAP (rate modifier, divide by 1000) (4.8)
	writeD(1000); // incItemUpgradeQina (rate modifier, divide by 1000) (4.8)
	writeC(0); // Augment disable or limit? 0/1 (4.8)
	writeF(3.0f); // exp rate modifier? (4.8)
	network::chatserver::ChatServer& chatServer = network::chatserver::ChatServer::getInstance();
	writeH(chatServer.getPublicIP()->length() > 0 ? 1 : 0); // ChatServersCount
	if (chatServer.getPublicIP()->length() > 0) {
		writeC(0); // spacer or maybe id
		std::vector<uint8_t> publicIp;
		for (int8_t octet : chatServer.getPublicIP()->snapshot())
			publicIp.push_back(static_cast<uint8_t>(octet));
		writeB(publicIp);
		writeH(chatServer.getPublicPort());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets

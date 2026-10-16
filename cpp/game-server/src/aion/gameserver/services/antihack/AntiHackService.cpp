#include "aion/gameserver/services/antihack/AntiHackService.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUIT_RESPONSE.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::services::antihack {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.antihack.AntiHackService");

namespace {

using configs::main::SecurityConfig;
using controllers::movement::MovementMask;

/**
 * Java Double.toString (JDK 19+: the shortest decimal that rounds to the value): plain notation with at least one fraction digit for
 * 1e-3 <= |d| < 1e7, otherwise computerized scientific notation ("1.0E7", "1.234E-5").
 */
std::string javaDoubleToString(double value) {
	if (std::isnan(value))
		return "NaN";
	if (std::isinf(value))
		return value > 0 ? "Infinity" : "-Infinity";
	if (value == 0)
		return std::signbit(value) ? "-0.0" : "0.0";
	char buffer[64];
	auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::scientific);
	static_cast<void>(error);
	std::string scientific(buffer, end); // e.g. "-1.2345e+02"
	bool negative = scientific.front() == '-';
	if (negative)
		scientific.erase(0, 1);
	size_t e = scientific.find('e');
	int exponent = std::atoi(scientific.c_str() + e + 1);
	std::string digits = scientific.substr(0, e);
	digits.erase(std::remove(digits.begin(), digits.end(), '.'), digits.end());
	std::string result;
	double magnitude = std::abs(value);
	if (magnitude >= 1e-3 && magnitude < 1e7) {
		if (exponent >= 0) {
			size_t integerDigits = static_cast<size_t>(exponent) + 1;
			if (digits.size() <= integerDigits)
				result = digits + std::string(integerDigits - digits.size(), '0') + ".0";
			else
				result = digits.substr(0, integerDigits) + "." + digits.substr(integerDigits);
		} else {
			result = "0." + std::string(static_cast<size_t>(-exponent - 1), '0') + digits;
		}
	} else {
		result = digits.substr(0, 1) + "." + (digits.size() > 1 ? digits.substr(1) : "0") + "E" + std::to_string(exponent);
	}
	return negative ? "-" + result : result;
}

std::string javaString(float value) {
	return geoEngine::math::JavaFloat::toString(value);
}

} // namespace

bool AntiHackService::canMove(model::gameobjects::player::Player& player, float x, float y, float z, int8_t type) {
	controllers::movement::PlayerMoveController& m = *player.getMoveController();
	runtime::Ptr<world::WorldPosition> lastPositionFromClient = m.getLastPositionFromClient();
	if (!lastPositionFromClient || lastPositionFromClient->getMapId() != player.getWorldId())
		return true;

	if (SecurityConfig::ABNORMAL.load()) {
		if (!player.canPerformMove() && !player.getEffectController()->isAbnormalSet(skillengine::effect::AbnormalState::PULLED) &&
			(type & MovementMask::GLIDE) != MovementMask::GLIDE) {
			if (player.abnormalHackCounter.get() > SecurityConfig::ABNORMAL_COUNTER.load()) {
				return punish(player, false, "possibly performed illegal move action (Anti-Abnormal Hack)");
			} else
				player.abnormalHackCounter.set(player.abnormalHackCounter.get() + 1);
		} else
			player.abnormalHackCounter.set(0);
	}

	float speed = player.getGameStats()->getMovementSpeedFloat();
	if (SecurityConfig::SPEEDHACK.load()) {
		if (type != 0) {
			if ((type & MovementMask::POSITION) == MovementMask::POSITION) {
				double vector2D = utils::PositionUtil::getDistance(x, y, m.getTargetX2(), m.getTargetY2());

				if (vector2D != 0) {
					if ((type & MovementMask::MANUAL) == MovementMask::MANUAL && vector2D > 5 && vector2D > speed + 0.001)
						player.speedHackCounter.set(player.speedHackCounter.get() + 1);
					else if (vector2D > 37.5 && vector2D > 1.5 * speed * speed + 0.001)
						player.speedHackCounter.set(player.speedHackCounter.get() + 1);
					else if (player.speedHackCounter.get() > 0)
						player.speedHackCounter.set(player.speedHackCounter.get() - 1);

					if (player.speedHackCounter.get() > SecurityConfig::SPEEDHACK_COUNTER.load()) {
						return punish(player, false,
							"possibly used speed hack - SHC:" + std::to_string(player.speedHackCounter.get()) + " S:" + javaString(speed) + " V:" +
								javaDoubleToString(std::rint(1000.0 * vector2D) / 1000.0) + " type:" + std::to_string(type));
					}
				}
			} else if ((type & MovementMask::ABSOLUTE) == MovementMask::ABSOLUTE && (type & MovementMask::GLIDE) != MovementMask::GLIDE) {
				double vector = utils::PositionUtil::getDistance(x, y, lastPositionFromClient->getX(), lastPositionFromClient->getY());
				int64_t timeDiff = commons::utils::currentTimeMillis() - m.getLastPositionFromClientMillis();

				if ((type & MovementMask::POSITION) == MovementMask::POSITION) {
					bool isMoveToTarget = false;
					runtime::Ptr<model::gameobjects::VisibleObject> target = player.getTarget();
					if (target && target.get() != &player) {
						double distDiff = utils::PositionUtil::getDistance(target->getX(), target->getY(), m.getTargetX2(), m.getTargetY2());
						isMoveToTarget = distDiff <= 5;
					}

					if (timeDiff > 1000 && player.speedHackCounter.get() > 0)
						player.speedHackCounter.set(player.speedHackCounter.get() - 1);

					if (vector > timeDiff * (speed + 0.85) * 0.001)
						player.speedHackCounter.set(player.speedHackCounter.get() + 1);
					else if (isMoveToTarget && player.speedHackCounter.get() > 0)
						player.speedHackCounter.set(player.speedHackCounter.get() - 1);
				} else if (vector > timeDiff * (speed + 0.25) * 0.001)
					player.speedHackCounter.set(player.speedHackCounter.get() + 1);
				else if (player.speedHackCounter.get() > 0)
					player.speedHackCounter.set(player.speedHackCounter.get() - 1);

				if (SecurityConfig::PUNISH.load() > 0 && player.speedHackCounter.get() > SecurityConfig::SPEEDHACK_COUNTER.load() + 5) {
					return punish(player, false,
						"possibly used speed hack - SHC:" + std::to_string(player.speedHackCounter.get()) + " SMS:" +
							javaDoubleToString(std::rint(100.0 * (timeDiff * (speed + 0.25) * 0.001)) / 100.0) + " TDF:" + std::to_string(timeDiff) +
							" VTD:" + javaDoubleToString(std::rint(1000.0 * (timeDiff * (speed + 0.85) * 0.001)) / 1000.0) + " VS:" +
							javaDoubleToString(std::rint(100.0 * vector) / 100.0) + " type:" + std::to_string(type));
				} else if (player.speedHackCounter.get() > SecurityConfig::SPEEDHACK_COUNTER.load()) {
					moveBack(player, false);
					return false;
				}
			}
		} else {
			double vector = utils::PositionUtil::getDistance(x, y, lastPositionFromClient->getX(), lastPositionFromClient->getY());
			int64_t timeDiff = commons::utils::currentTimeMillis() - m.getLastPositionFromClientMillis();

			if (m.getLastMovementMask() == 0 && vector > timeDiff * speed * 0.00075)
				player.speedHackCounter.set(player.speedHackCounter.get() + 1);

			if (SecurityConfig::PUNISH.load() > 0 && player.speedHackCounter.get() > SecurityConfig::SPEEDHACK_COUNTER.load() + 5) {
				return punish(player, false,
					"possibly used speed hack - SHC:" + std::to_string(player.speedHackCounter.get()) + " TD:" +
						javaDoubleToString(std::rint(1000.0 * static_cast<double>(timeDiff)) / 1000.0) + " VTD:" +
						javaDoubleToString(std::rint(1000.0 * (timeDiff * speed * 0.00075)) / 1000.0) + " VS:" +
						javaDoubleToString(std::rint(100.0 * vector) / 100.0) + " type:" + std::to_string(type));
			} else if (player.speedHackCounter.get() > SecurityConfig::SPEEDHACK_COUNTER.load() + 2) {
				moveBack(player, false);
				return false;
			}
		}
	}

	if (SecurityConfig::TELEPORTATION.load()) {
		double delta = utils::PositionUtil::getDistance(x, y, player.getX(), player.getY()) / speed;
		if (speed > 5.0 && delta > 5.0 && (type & MovementMask::GLIDE) != MovementMask::GLIDE) {
			return punish(player, true,
				"possibly used teleport hack - S:" + javaString(speed) + " D:" + javaDoubleToString(std::rint(1000.0 * delta) / 1000.0) + " type:" +
					std::to_string(type));
		}
	}

	return true;
}

bool AntiHackService::punish(model::gameobjects::player::Player& player, bool normalMovePacket, std::string_view message) {
	utils::audit::AuditLogger::log(player, message);
	switch (SecurityConfig::PUNISH.load()) {
		case 1:
			moveBack(player, normalMovePacket);
			return false;
		case 2:
			moveBack(player, normalMovePacket);
			if (player.speedHackCounter.get() > SecurityConfig::SPEEDHACK_COUNTER.load() * 3 ||
				player.abnormalHackCounter.get() > SecurityConfig::ABNORMAL_COUNTER.load() * 3)
				player.getClientConnection()->close(network::aion::serverpackets::SM_QUIT_RESPONSE());
			return false;
		case 3:
			player.getClientConnection()->close(network::aion::serverpackets::SM_QUIT_RESPONSE());
			return false;
		default:
			return true;
	}
}

void AntiHackService::moveBack(model::gameobjects::player::Player& player, bool normalMovePacket) {
	if (normalMovePacket)
		utils::PacketSendUtility::broadcastPacketAndReceive(player, network::aion::serverpackets::SM_MOVE(player));
	else {
		runtime::Ptr<world::WorldPosition> lastPos = player.getMoveController()->getLastPositionFromClient();
		utils::PacketSendUtility::broadcastPacketAndReceive(player,
			network::aion::serverpackets::SM_FORCED_MOVE(player, player.getObjectId(), lastPos->getX(), lastPos->getY(), lastPos->getZ()));
	}
	player.getMoveController()->updateLastMove();
	player.speedHackCounter.set(0);
}

void AntiHackService::checkAionBin(int32_t size, network::aion::AionConnection* con) {
	int32_t legitSize = 212; // 212 after login, exactly 30 minutes later: 224, right after that: 1128 o.O
	if (SecurityConfig::AION_BIN_CHECK.load()) {
		if (size != legitSize) {
			log.warn("Detected modified aion.bin for account ID " + std::to_string(con->getAccount()->getId()));
			con->close(network::aion::serverpackets::SM_QUIT_RESPONSE());
		}
	}
	// con.sendPacket(new SM_GAMEGUARD(size)); // not sent on GF servers currently
}

} // namespace aion::gameserver::services::antihack

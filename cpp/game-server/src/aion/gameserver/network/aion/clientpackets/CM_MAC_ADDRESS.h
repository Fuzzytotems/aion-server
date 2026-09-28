#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * In this packet client is sending connection and system information (IPs, MAC Address and HDD serial).
 *
 * @author -Nemesiss-, KID, ViAl, Neon
 */
class CM_MAC_ADDRESS : public AionClientPacket {
private:
	std::string macAddress;
	std::string hddSerial;

public:
	CM_MAC_ADDRESS(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;

public:
	/**
	 * Java: private static String fixHddSerial(String) (public in C++ for the byte vector tests). Works on the UTF-16 code units of the text like
	 * Java's String: the length check, the character class checks, the UTF-16LE hex dump and the pairwise swap. readS already replaced unpaired
	 * surrogates with U+FFFD, which is what Java's String.getBytes(UTF_16LE) writes for them (bytes FD FF), so the hex dump is the same.
	 */
	static std::string fixHddSerial(std::string_view hddSerial);
};

} // namespace aion::gameserver::network::aion::clientpackets

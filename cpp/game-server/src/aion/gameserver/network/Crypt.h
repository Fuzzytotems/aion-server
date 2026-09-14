#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "aion/gameserver/network/EncryptionKeyPair.h"

namespace aion::gameserver::network {

/**
 * Crypt will encrypt server packet and decrypt client packet.
 * <p>
 * Wire format of the game client connection (little endian; the 2-byte length includes itself and is never encrypted):
 * <pre>
 * server frame: [u16 length][u16 encodeServerPacketOpcode(opcode)][u8 0x44][u16 ~encoded opcode][body]
 * client frame: [u16 length][u16 wire opcode][u8 0x65][u16 ~wire opcode][body], decodeClientPacketOpcode(wire opcode) = opcode
 * </pre>
 * The first server packet (SM_KEY) calls enableKey() while it is written and is sent unencrypted: the first encrypt() call only enables the
 * crypt. Every later server packet and every client packet received after that is encrypted with EncryptionKeyPair.
 * <p>
 * Thread safety: none, like Java. The connection uses it only on its IO strand (SM_KEY is written there, see runtime-architecture.md §8.6).
 * <p>
 * Java: com.aionemu.gameserver.network.Crypt
 *
 * @author hack99, kao, -Nemesiss-
 */
class Crypt {
public:
	/** Second byte of server packet must be equal to this (the byte at index 2 after the length) */
	static constexpr uint8_t staticServerPacketCode = 0x44;

	/**
	 * Version number used for client version validation and opcode obfuscation (aion 4.8.0.0 = 207).
	 * <p>
	 * Java: SM_VERSION_CHECK.INTERNAL_VERSION (the tests check it against the generated network/aion/ServerPacketsOpcodes.gen.h)
	 */
	static constexpr int32_t INTERNAL_VERSION = 207;

	/**
	 * Enable crypt key - generate random key that will be used to encrypt second server packet [first one is unencrypted] and decrypt client
	 * packets. This method is called from SM_KEY server packet, that packet sends key to aion client.
	 *
	 * @return Enciphered key which the client will decipher and use for all future (encrypted) communication
	 * @throws commons::utils::IllegalStateException if the key is already initialized
	 */
	int32_t enableKey();

	/** C++ addition: enableKey() with a given instead of a random base key (deterministic tests). Java: the body of enableKey(). */
	int32_t enableKey(int32_t key);

	bool isEnabled() const noexcept { return enabled; }

	/**
	 * Decrypt client packet in place (the packet body after the length, see EncryptionKeyPair::decrypt).
	 *
	 * @return true if decryption was successful.
	 * @throws commons::utils::IllegalStateException if no key was enabled (Java: NullPointerException)
	 */
	bool decrypt(std::span<uint8_t> data);

	/**
	 * Encrypt server packet in place (everything after the length). The first call only enables the crypt and leaves the data unencrypted.
	 *
	 * @throws commons::utils::IllegalStateException if the crypt is enabled but no key was enabled (Java: NullPointerException)
	 */
	void encrypt(std::span<uint8_t> data);

	/** C++ addition for tests and debugging: the key pair, or nullptr before enableKey() */
	const EncryptionKeyPair* getPacketKey() const noexcept { return packetKey ? &*packetKey : nullptr; }

	/** @return Obfuscated server packet opcode: (opcode + INTERNAL_VERSION) ^ 0xDF (int arithmetic; the frame stores the low 16 bits) */
	static constexpr int32_t encodeServerPacketOpcode(int32_t opcode) noexcept {
		return static_cast<int32_t>((static_cast<uint32_t>(opcode) + static_cast<uint32_t>(INTERNAL_VERSION)) ^ 0xDFu);
	}

	/**
	 * @return Deobfuscated client packet opcode: ((opcode ^ 0xEF) - 0xC ^ 0xEF) - INTERNAL_VERSION, where Java's precedence applies the
	 *         subtraction before the second XOR (int arithmetic; AionClientPacketFactory passes the unsigned 16-bit wire value)
	 */
	static constexpr int32_t decodeClientPacketOpcode(int32_t opcode) noexcept {
		return static_cast<int32_t>((((static_cast<uint32_t>(opcode) ^ 0xEFu) - 0xCu) ^ 0xEFu) - static_cast<uint32_t>(INTERNAL_VERSION));
	}

private:
	/** Crypt is enabled after first server packet was send. (Java: isEnabled) */
	bool enabled = false;

	std::optional<EncryptionKeyPair> packetKey;
};

} // namespace aion::gameserver::network

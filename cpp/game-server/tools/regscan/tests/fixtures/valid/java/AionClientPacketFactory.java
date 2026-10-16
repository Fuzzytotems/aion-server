package com.aionemu.gameserver.network.aion;

public class AionClientPacketFactory {

	private static final PacketInfo<? extends AionClientPacket>[] packets = new PacketInfo<?>[250];

	static {
		try {
			packets[3] = new PacketInfo<>(CM_QUIT.class, State.AUTHED, State.IN_GAME); // [C_ASK_QUIT (AskQuitPacket)]
			// packets[6] = new PacketInfo<>(CM_COMMENTED_OUT.class, State.IN_GAME);
			packets[9] = new PacketInfo<>(CM_LEVEL_READY.class, State.IN_GAME);
			packets[48] = new PacketInfo<>(CM_MOVE.class, State.IN_GAME); // [C_MOVE_NEW (MoveNewPacket)]
		} catch (NoSuchMethodException e) {
			throw new ExceptionInInitializerError(e);
		}
	}
}

package com.aionemu.gameserver.network.aion;

import java.nio.ByteBuffer;

public abstract class AionServerPacket {

	protected AionServerPacket(int opCode) {
	}

	protected abstract void writeImpl(AionConnection con);

	protected void writeBuffer(ByteBuffer buf) {
	}
}

package ai;

import com.aionemu.gameserver.ai.AIName;
import com.aionemu.gameserver.ai.NpcAI;
import com.aionemu.gameserver.model.gameobjects.Npc;

@AIName("general")
public class GeneralNpcAI extends NpcAI {

	public GeneralNpcAI(Npc owner) {
		super(owner);
	}
}

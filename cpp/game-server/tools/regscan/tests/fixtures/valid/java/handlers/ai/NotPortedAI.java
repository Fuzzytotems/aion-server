package ai;

import com.aionemu.gameserver.ai.AIName;
import com.aionemu.gameserver.model.gameobjects.Npc;

/**
 * @AIName("javadoc_is_ignored")
 */
@AIName("not_ported")
public class NotPortedAI extends GeneralNpcAI {

	public NotPortedAI(Npc owner) {
		super(owner);
	}

	private void summon() {
		sp(800001, 1f, 2f, 3f, (byte) 0, 0, "walker");
	}
}

#include "Common.h"
#include "GossipDef.h"
#include "Chat.h"
#include "Item.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

class Transpolyporter : public ItemScript  {
public:
    Transpolyporter() : ItemScript("Transpolyporter") {}

    /*
        - Character unlocks are handled via items "Keys"
        - (Ethereal Network) "DB" = Order, MapId, X, Y, Z, O
        - (Personal Network) "DB" = Order, GUID, MapId, X, Y, Z, O
        - Keys are obtained from gateway guardians.
    */

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override {
        if (!player->GetMap()->IsBattlegroundOrArena() && !player->IsInCombat() && !player->IsInFlight()) {
            ChatHandler(player->GetSession()).PSendSysMessage("You cannot use this device while in combat, flight, or while in a battleground or arena!");
            return true;
        }

        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_DOT, "Ethereal Network", GOSSIP_SENDER_MAIN, 1);
        AddGossipItemFor(player, GOSSIP_ICON_DOT, "Personal Network", GOSSIP_SENDER_MAIN, 2);
        SendGossipMenuFor(player, 1, item->GetGUID());
        return true;
    }

    void OnGossipSelect(Player* player, Item* /*item*/, uint32 /*sender*/, uint32 action) override {
        ClearGossipMenuFor(player);

        switch (action) {
            case 1:
                CloseGossipMenuFor(player);
                break;
        }
    }
};

void AddSC_Transpolyporter() {
    new Transpolyporter();
}

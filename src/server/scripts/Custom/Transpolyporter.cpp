#include "Common.h"
#include "GossipDef.h"
#include "Chat.h"
#include "Item.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "WorldSession.h"
#include "DatabaseEnv.h"

struct TeleportLocation {
    std::string icon_name;
    std::string name;
    uint32 map_id;
    float x;
    float y;
    float z;
    float o;
    uint32 key_id;
};

std::map<uint32, TeleportLocation> etherealNetwork;

class Transpolyporter_Server : public WorldScript {
public:
    Transpolyporter_Server() : WorldScript("Transpolyporter_Server") {}

    void OnStartup() override {
        QueryResult ethereal_network = WorldDatabase.Query("SELECT icon_name, network_name, map_id, x, y, z, o, key_id FROM z_ethereal_network");

        if (!ethereal_network)
            return;

        uint32 locationId = 0;
        do {
            Field* fields = ethereal_network->Fetch();

            TeleportLocation loc;
            loc.icon_name = fields[0].GetString();
            loc.name = fields[1].GetString();
            loc.map_id = fields[2].GetUInt32();
            loc.x = fields[3].GetFloat();
            loc.y = fields[4].GetFloat();
            loc.z = fields[5].GetFloat();
            loc.o = fields[6].GetFloat();
            loc.key_id = fields[7].GetUInt32();

            etherealNetwork[locationId++] = loc;

        } while (ethereal_network->NextRow());
    };
};

class Transpolyporter : public ItemScript  {
public:
    Transpolyporter() : ItemScript("Transpolyporter") {}

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override {
        if (player->GetMap()->IsBattlegroundOrArena() && player->IsInCombat() && player->IsInFlight()) {
            ChatHandler(player->GetSession()).PSendSysMessage("You cannot use this device while in combat, flight, or while in a battleground or arena!");
            return true;
        }

        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_DOT, "Ethereal Network", GOSSIP_SENDER_MAIN, 1);
        AddGossipItemFor(player, GOSSIP_ICON_DOT, "Personal Network", GOSSIP_SENDER_MAIN, 2);
        SendGossipMenuFor(player, 1, item->GetGUID());
        return true;
    }

    void OnGossipSelect(Player* player, Item* item, uint32 /*sender*/, uint32 action) override {
        ClearGossipMenuFor(player);

        switch (action) {
            case 1:
                if (etherealNetwork.empty()) {
                    CloseGossipMenuFor(player);
                    return;
                }

                for (const auto& pair : etherealNetwork) {
                    uint32 locationId = pair.first;
                    const TeleportLocation& loc = pair.second;

                    // TODO: Add the check for whether or not the loc is unlocked.

                    AddGossipItemFor(player, GOSSIP_ICON_DOT, loc.icon_name + loc.name, GOSSIP_SENDER_MAIN, 100 + locationId);
                }

                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back", GOSSIP_SENDER_MAIN, 0);
                SendGossipMenuFor(player, 1, item->GetGUID());
                break;

            case 2:
                // TODO: Add the personal network system.
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back", GOSSIP_SENDER_MAIN, 0);
                SendGossipMenuFor(player, 1, item->GetGUID());
                break;

            case 0:
                OnUse(player, item, SpellCastTargets());
                break;

            default:
                if (action >= 100) {
                    uint32 locationId = action - 100;

                    if (etherealNetwork.find(locationId) != etherealNetwork.end()) {
                        const TeleportLocation& loc = etherealNetwork[locationId];
                        player->TeleportTo(loc.map_id, loc.x, loc.y, loc.z, loc.o);
                    }
                }
                CloseGossipMenuFor(player);
                break;
        }
    }
};

void AddSC_Transpolyporter() {
    new Transpolyporter();
    new Transpolyporter_Server();
}

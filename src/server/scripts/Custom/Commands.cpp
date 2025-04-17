#include "ScriptMgr.h"
#include "Chat.h"
#include "Language.h"
#include "Player.h"
#include "WorldSession.h"

using namespace rbac;
using namespace std;

class Commands : public CommandScript {
public:
    Commands() : CommandScript("Commands") {}

    vector<ChatCommand> GetCommands() const override {
        static vector<ChatCommand> CommandsTable = {
            { "test", RBAC_PERM_COMMAND_GM_ONLY, false, &HandleCustomCommand, "" }
        };

        return CommandsTable;
    }

    static bool HandleCustomCommand(ChatHandler* handler, const char* /*args*/) {
        Player* player = handler->GetSession()->GetPlayer();
        ChatHandler(player->GetSession()).PSendSysMessage("It Works.");
        return true;
    }
};

void AddSC_Commands() {
    new Commands();
}

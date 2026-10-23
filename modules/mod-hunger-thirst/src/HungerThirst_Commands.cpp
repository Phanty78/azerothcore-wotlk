/*
 * Faim et soif - command script.
 *
 * Exposes the `.faim` command:
 *   .faim / .faim status                  show the player's current state
 *   .faim aide                            short French help
 *   .faim set <player> <satiety> <hydration>  game master only
 *   .faim reset <player>                  game master only
 *
 * The command owns no state: every read and write goes through the
 * HungerThirst contract declared in HungerThirst.h.
 */

#include "Chat.h"
#include "ChatCommandTags.h"
#include "CommandScript.h"
#include "Common.h"
#include "HungerThirst.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "StringConvert.h"
#include "Tokenize.h"
#include "Util.h"
#include "WorldSession.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace HungerThirst
{
namespace
{
    bool IsGameMaster(ChatHandler* handler)
    {
        WorldSession* session = handler->GetSession();
        return session && session->GetSecurity() >= SEC_GAMEMASTER;
    }

    bool RejectWithoutGameMaster(ChatHandler* handler)
    {
        handler->PSendSysMessage("Cette commande est réservée aux maîtres de jeu.");
        handler->SetSentErrorMessage(true);
        return false;
    }

    bool EnsureModuleEnabled(ChatHandler* handler)
    {
        if (GetConfig().Enable)
            return true;

        handler->PSendSysMessage("Le module Faim & Soif est désactivé sur ce serveur.");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* FindOnlineTarget(ChatHandler* handler, std::string_view name)
    {
        Player* target = ObjectAccessor::FindPlayerByName(std::string(name));
        if (!target)
        {
            handler->PSendSysMessage("Le joueur '{}' est introuvable ou hors ligne.", std::string(name));
            handler->SetSentErrorMessage(true);
        }

        return target;
    }

    bool SendHelp(ChatHandler* handler)
    {
        Config const& config = GetConfig();

        handler->PSendSysMessage("Faim & Soif - commandes :");
        handler->PSendSysMessage("  .faim, .faim status : affiche votre satiété et votre hydratation.");
        handler->PSendSysMessage("  .faim aide : affiche cette aide.");
        handler->PSendSysMessage(
            "  .faim set <joueur> <satiété> <hydratation> : fixe les valeurs, "
            "bornées de 0 à 100 (maître de jeu).");
        handler->PSendSysMessage("  .faim reset <joueur> : réinitialise l'état d'un joueur (maître de jeu).");
        handler->PSendSysMessage("Seuils : alerte à {} %, critique à {} %. Mode actif : {}.",
            config.WarningThreshold, config.CriticalThreshold, ModeName(config.CurrentMode));
        return true;
    }

    bool HandleSetCommand(ChatHandler* handler, std::vector<std::string_view> const& tokens)
    {
        if (!IsGameMaster(handler))
            return RejectWithoutGameMaster(handler);

        if (!EnsureModuleEnabled(handler))
            return false;

        if (tokens.size() != 4)
        {
            handler->PSendSysMessage("Syntaxe : .faim set <joueur> <satiété> <hydratation>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* target = FindOnlineTarget(handler, tokens[1]);
        if (!target)
            return false;

        Optional<double> satiety = Acore::StringTo<double>(tokens[2]);
        Optional<double> hydration = Acore::StringTo<double>(tokens[3]);
        if (!satiety || !hydration || !std::isfinite(*satiety) || !std::isfinite(*hydration))
        {
            handler->PSendSysMessage("Les valeurs de satiété et d'hydratation doivent être des nombres.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        double const clampedSatiety = std::clamp(*satiety, 0.0, 100.0);
        double const clampedHydration = std::clamp(*hydration, 0.0, 100.0);

        SetState(target, clampedSatiety, clampedHydration);
        SaveState(target, true);
        SendAddonState(target);

        char const* persistenceNote = IsPersistenceReady() ? "" : " (non persisté : base indisponible)";
        handler->PSendSysMessage("État de {} défini : satiété {:.1f}/100, hydratation {:.1f}/100{}.",
            target->GetName(), clampedSatiety, clampedHydration, persistenceNote);
        return true;
    }

    bool HandleResetCommand(ChatHandler* handler, std::vector<std::string_view> const& tokens)
    {
        if (!IsGameMaster(handler))
            return RejectWithoutGameMaster(handler);

        if (!EnsureModuleEnabled(handler))
            return false;

        if (tokens.size() != 2)
        {
            handler->PSendSysMessage("Syntaxe : .faim reset <joueur>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* target = FindOnlineTarget(handler, tokens[1]);
        if (!target)
            return false;

        ResetState(target);
        SaveState(target, true);
        SendAddonState(target);

        char const* persistenceNote = IsPersistenceReady() ? "" : " (non persisté : base indisponible)";
        handler->PSendSysMessage("État faim/soif de {} réinitialisé{}.", target->GetName(), persistenceNote);
        return true;
    }
}

class HungerThirst_CommandScript : public CommandScript
{
public:
    HungerThirst_CommandScript() : CommandScript("HungerThirst_CommandScript") { }

    [[nodiscard]] Acore::ChatCommands::ChatCommandTable GetCommands() const override
    {
        static Acore::ChatCommands::ChatCommandTable commandTable =
        {
            { "faim", HandleFaimCommand, SEC_PLAYER, Acore::ChatCommands::Console::No }
        };

        return commandTable;
    }

    static bool HandleFaimCommand(ChatHandler* handler, Acore::ChatCommands::Tail args)
    {
        Player* player = handler->GetPlayer();
        if (!player)
        {
            handler->PSendSysMessage("Cette commande doit être utilisée en jeu.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string_view const input = args;
        std::vector<std::string_view> const tokens = Acore::Tokenize(input, ' ', false);

        if (tokens.empty() || StringEqualI(tokens[0], "status"))
        {
            SendStatus(handler, player);
            return true;
        }

        if (StringEqualI(tokens[0], "aide"))
            return SendHelp(handler);

        if (StringEqualI(tokens[0], "set"))
            return HandleSetCommand(handler, tokens);

        if (StringEqualI(tokens[0], "reset"))
            return HandleResetCommand(handler, tokens);

        handler->PSendSysMessage("Sous-commande inconnue : '{}'. Utilisez .faim aide.", tokens[0]);
        handler->SetSentErrorMessage(true);
        return false;
    }
};

void AddHungerThirstCommandScript()
{
    new HungerThirst_CommandScript();
}
}

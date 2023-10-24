/**
 * @date Unknown
 * @author JosByte
 * @defgroup NpcBountyHunt Bounty Hunt
 * @brief
 * The "Npc Bounty Hunt" allows players to gain credits instantly by killing NPCs and tractoring the bounty item.
 *
 * @paragraph cmds Player Commands
 *
 * There are no player commands in this plugin.
 *
 * @paragraph adminCmds Admin Commands
 * There are no admin commands in this plugin.
 *
 * @paragraph configuration Configuration
 * @code
 * {
 *     "enableBountyHunt": true,
 * }
 * @endcode
 *
 * @paragraph ipc IPC Interfaces Exposed
 * This plugin does not expose any functionality.
 *
 * @paragraph optional Optional Plugin Dependencies
 * None
 */

#include "NpcBounty.h"
#include <iostream>
#include <nlohmann/json.hpp>

bool set_bBountiesEnabled = true;

namespace Plugins::NpcBounty
{
	const std::unique_ptr<Global> global = std::make_unique<Global>();

	/** @ingroup NpcBountyHunt
	 *
	 */
	void ShipDestroyed(DamageList* _dmg, const DWORD** ecx, const uint& kill)
	{

		const DamageList* dmg = _dmg;
		auto is_inflictor_player = dmg->is_inflictor_a_player();
		uint inflictor_player = dmg->get_inflictor_owner_player();

		CShip* cShip = Hk::Player::CShipFromShipDestroyed(ecx);

		if (!kill || !is_inflictor_player || inflictor_player > 9999999)
		{
			return;
		}

		int npcRep;
		pub::SpaceObj::GetRep(cShip->get_id(), npcRep);

		uint Affiliation;
		pub::Reputation::GetAffiliation(npcRep, Affiliation);

		int playerRep;
		pub::Player::GetRep(inflictor_player, playerRep);

		float playerFactionRep;
		pub::Reputation::GetGroupFeelingsTowards(playerRep, Affiliation, playerFactionRep);


		if (playerFactionRep > -0.75)
		{
			PrintUserCmdText(inflictor_player, L"Target is too ally for claiming bounty.");
			return;
		}

		Archetype::Root const* ship = cShip->shiparch();

		const char* shipCMP = ship->szName;

		std::string fullName(shipCMP);
		std::string::size_type lastSlash = fullName.find_last_of('\\');

		std::string extractedName = fullName.substr(lastSlash + 1);

		size_t extensionPos = extractedName.find(".cmp");
		if (extensionPos != std::string::npos) //get ship name without .cmp
		{
			extractedName = extractedName.substr(0, extensionPos);
		}

		std::ifstream file("./config/NPCBountyHunt/bounties.json");

		if (!file.is_open())
		{
			return;
		}

		nlohmann::json jsonData;
		file >> jsonData;

		std::string searchName = std::string(extractedName.begin(), extractedName.end());

		for (const auto& ship : jsonData["ships"])
		{
			std::string shipName = ship["ship_name"];
			uint price = ship["bounty"];

			if (shipName == searchName)
			{
				auto testRep = Hk::Solar::GetAffiliation(cShip->get_id());
				auto testRep2 = Hk::Player::GetRep(inflictor_player, testRep.value());

				Hk::Player::AddCash(inflictor_player, price);
				PrintUserCmdText(inflictor_player, L"Bounty claimed: " + std::to_wstring(price) + L"$"); 
				break;
			}
		}
	}

	// Client command processing

} // namespace Plugins::NpcBountyHunt

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FLHOOK STUFF
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace Plugins::NpcBounty;

extern "C" EXPORT void ExportPluginInfo(PluginInfo* pi)
{
	pi->name("Npc Bounty");
	pi->shortName("npcbounty");
	pi->mayUnload(true);
	pi->returnCode(&global->returnCode);
	pi->versionMajor(PluginMajorVersion::VERSION_04);
	pi->versionMinor(PluginMinorVersion::VERSION_00);
	pi->emplaceHook(HookedCall::IEngine__ShipDestroyed, &ShipDestroyed);
}
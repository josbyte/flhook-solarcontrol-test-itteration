﻿/**
 * @date July, 2023
 * @author Raikkonen
 * @defgroup WaveDefence Wave Defence
 * @brief
 * This plugin will spawn wave of enemies against the players and reward after each round. Think Call of Duty Zombies or SpecOps.
 *
 * @paragraph cmds Player Commands
 * None
 *
 * @paragraph adminCmds Admin Commands
 * None
 *
 * @paragraph configuration Configuration
 * @code
 * @endcode
 *
 * @paragraph ipc IPC Interfaces Exposed
 * This plugin does not expose any functionality.
 * 
 * @paragraph ipc IPC Interfaces Used
 * NpcCommunicator: uses CreateNpc method with parameters (const std::wstring& name, Vector position, const Matrix& rotation, SystemId systemId, bool varyPosition)
 * SolarCommunicator: uses CreateUserDefinedSolar method with parameters (const std::wstring& name, Vector pos, const Matrix& rot, uint iSystem, bool varyPosition, bool mission)
 */

#include "WaveDefence.h"

namespace Plugins::WaveDefence
{
	const std::unique_ptr<Global> global = std::make_unique<Global>();

	/** @ingroup WaveDefence
	 * @brief Load plugin settings into memory
	 */
	void LoadSettings()
	{
		auto config = Serializer::JsonToObject<Config>();

		config.victoryMusicId.iDunno = 0;
		config.victoryMusicId.iMusicId = CreateID(config.victoryMusic.c_str());

		config.failureMusicId.iDunno = 0;
		config.failureMusicId.iMusicId = CreateID(config.failureMusic.c_str());

		for (auto& system : config.systems)
		{
			// System & Position
			system.positionVector.x = system.posX;
			system.positionVector.y = system.posY;
			system.positionVector.z = system.posZ;
			system.systemId = CreateID(system.system.c_str());

			// Waves
			for (auto& wave : system.waves)
			{
				wave.startVoiceLine.voiceLine.id = CreateID(wave.startVoiceLine.voiceLineString.c_str());
				wave.endVoiceLine.voiceLine.id = CreateID(wave.endVoiceLine.voiceLineString.c_str());
			}
		}

		// Character / Costume
		for (auto& character : config.characters)
		{
			character.voiceId = CreateID(character.voice.c_str());
			character.costume.head = CreateID(character.costumeStrings.head.c_str());
			character.costume.body = CreateID(character.costumeStrings.body.c_str());
			character.costume.lefthand = CreateID(character.costumeStrings.lefthand.c_str());
			character.costume.righthand = CreateID(character.costumeStrings.righthand.c_str());
			character.costume.accessories = character.costumeStrings.accessories;

			for (uint i = 0; i < character.costumeStrings.accessory.size(); i++)
			{
				character.costume.accessory[i] = CreateID(character.costumeStrings.accessory[i].c_str());
			}
		}

		global->config = std::make_unique<Config>(config);
	}

	/** @ingroup WaveDefence
	 * @brief Sends a comm to the group
	 */
	void SendComm(const std::vector<uint>& group, const VoiceLine& voiceLine)
	{
		auto character = std::ranges::find_if(
		    global->config->characters.begin(), global->config->characters.end(), [&voiceLine](auto& item) { return voiceLine.character == item.voice; });

		if (character != global->config->characters.end())
		{
			for (auto const& player : group)
			{
				uint ship;
				pub::Player::GetShip(player, ship);
				pub::SpaceObj::SendComm(
				    0, ship, character->voiceId, &character->costume, character->infocard, (uint*)&voiceLine.voiceLine, 9, 19009, 0.5, false);
			}
		}
	}

	/** @ingroup WaveDefence
	 * @brief Shows an infocard via mission messages. Candidate for helper function in the future.
	 */
	void ShowPlayerMissionText(uint client, uint infocard, MissionMessageType type)
	{
		FmtStr caption(0, nullptr);
		caption.begin_mad_lib(infocard);
		caption.end_mad_lib();

		pub::Player::DisplayMissionMessage(client, caption, type, true);
	}

	/** @ingroup WaveDefence
	 * @brief Checks that the player is able to join a wave defence game
	 */
	bool PlayerChecks(uint client, uint member, uint system)
	{
		auto characterName = Hk::Client::GetCharacterNameByID(member);

		// Check player is in the correct system
		uint playerSystem;
		pub::Player::GetSystem(member, playerSystem);
		if (playerSystem != system)
		{
			PrintUserCmdText(client, std::format(L"{} needs to be in the System to start a Wave Defence game.", characterName.value()));
			return false;
		}

		// Check player is in space
		uint ship;
		pub::Player::GetShip(member, ship);
		if (!ship)
		{
			PrintUserCmdText(client, std::format(L"{} needs to be in space to start a Wave Defence game.", characterName.value()));
			return false;
		}

		// Is the player already in a game?
		for (auto& game : global->games)
		{
			if (std::ranges::find(game.members, member) != game.members.end())
			{
				PrintUserCmdText(client, std::format(L"{} is already in a Wave Defence game.", characterName.value()));
				return false;
			}
		}

		return true;
	}

	/** @ingroup WaveDefence
	 * @brief Ends the wave defence game either successfully or not
	 */
	void EndGame(Game& game, bool success)
	{
		// Success Announcement
		if (success)
		{
			auto player = Hk::Client::GetCharacterNameByID(game.members.front());
			const auto message = Hk::Message::FormatMsg(
			    MessageColor::Red, MessageFormat::Normal, std::format(L"{} and their team have completed a Wave Defence game.", player.value()));
			Hk::Message::FMsgU(message);

			// Play music and show victory mission text for each player
			for (const auto& member : game.members)
			{
				ShowPlayerMissionText(member, 21650, MissionMessageType::MissionMessageType_Type3);
				pub::Audio::SetMusic(member, global->config->victoryMusicId);
			}
		}

		// Remove game from global
		auto gameSubRange = std::ranges::remove_if(global->games, [&game](auto& g) { return g.system.systemId == game.system.systemId; });
		global->games.erase(gameSubRange.begin(), gameSubRange.end());
	}

	/** @ingroup WaveDefence
	 * @brief Starts a new wave for the specified game
	 */
	void NewWave(Game& game)
	{
		// Get wave
		Wave const& wave = game.system.waves.at(game.waveNumber);

		// Check IPC is working
		if (!global->npcCommunicator || !global->solarCommunicator)
		{
			AddLog(LogType::Normal, LogLevel::Err, "Cannot communicate with dependent plugins! Check they are loaded.");
			PrintUserCmdText(game.members.front(), L"There has been an error with the Wave Defence plugin. Please contact an administrator.");
			EndGame(game, false);
			return;
		}

		// Get current location to know where to spawn the npcs and solars
		uint ship;
		Vector position;
		Matrix rotation;
		pub::Player::GetShip(game.members.front(), ship);
		pub::SpaceObj::GetLocation(ship, position, rotation);

		// Spawn specific npcs
		for (auto const& npc : wave.npcs)
		{
			game.spawnedNpcs.push_back(global->npcCommunicator->CreateNpc(npc, game.system.positionVector, rotation, game.system.systemId, true));
		}

		// Spawn variable npcs. This scales up depending on how many players there are
		for (uint multiple = 0; multiple < global->config->npcMultiplier; multiple++)
		{
			for (auto const& npc : wave.variableNpcs)
			{
				game.spawnedNpcs.push_back(global->npcCommunicator->CreateNpc(npc, game.system.positionVector, rotation, game.system.systemId, true));
			}
		}

		// Spawn solars
		for (auto const& solar : wave.solars)
		{
			game.spawnedSolars.push_back(
			    global->solarCommunicator->CreateSolar(solar, game.system.positionVector, rotation, game.system.systemId, true, true));
		}

		// Actions for all players in group
		for (auto const& player : game.members)
		{
			// Defend yourself!
			ShowPlayerMissionText(player, 22612, MissionMessageType_Type2);

			int reputation;
			pub::Player::GetRep(player, reputation);

			// Set all enemies to be hostile
			for (auto const& npc : game.spawnedNpcs)
			{
				int npcReputation;
				pub::SpaceObj::GetRep(npc, npcReputation);
				pub::Reputation::SetAttitude(npcReputation, reputation, -0.9f);
			}
		}

		// Send voice line
		SendComm(game.members, wave.startVoiceLine);
	}

	/** @ingroup WaveDefence
	 * @brief Starts a new wave defence game
	 */
	void NewGame(ClientId& client, [[maybe_unused]] const std::wstring& param)
	{
		// Grab the players system
		uint systemId;
		pub::Player::GetSystem(client, systemId);

		// Is there a game already in this system?
		for (auto const& game : global->games)
		{
			if (game.system.systemId == systemId)
			{
				PrintUserCmdText(client, L"There is already a Survival game occuring in this System. Go to another System or wait until the game is over");
				return;
			}
		}

		// Init game struct
		Game game;

		// Is a survival game possible in this system?
		for (auto const& system : global->config->systems)
		{
			if (systemId == system.systemId)
			{
				game.system = system;
				break;
			}
		}
		if (game.system.systemId == 0)
		{
			PrintUserCmdText(client, L"There isn't an available game in this System.");
			return;
		}

		// Is there anyone in the system who isn't in the group?
		PlayerData* playerData = nullptr;
		while (playerData = Players.traverse_active(playerData))
		{
			uint playerSystemId = 0;
			pub::Player::GetSystem(playerData->iOnlineId, playerSystemId);

			if (systemId != playerSystemId)
			{
				PrintUserCmdText(client, L"There are players in the System who are not part of your group.");
				return;
			}
		}

		// Is player in a group?
		if (uint groupId = Players.GetGroupID(client); groupId != 0)
		{
			// Get players in the group
			st6::vector<unsigned int> tempList;
			CPlayerGroup::FromGroupID(groupId)->StoreMemberList(tempList);
			for (auto const& player : tempList)
				game.members.push_back(player);
		}
		else
			// Store just the player
			game.members.push_back(client);

		// Check each member of the group passes the checks
		for (auto const& player : game.members)
		{
			if (!PlayerChecks(client, player, systemId))
				return;
		}

		// Announce the game start and push the game into the vector
		PrintLocalUserCmdText(game.members.front(), L"The game will start shortly.", 5000);
		global->games.push_back(game);
	}

	/** @ingroup WaveDefence
	 * @brief Ends the current wave for the game
	 */
	void EndWave(Game& game)
	{
		// Get Wave
		const Wave& wave = game.system.waves.at(game.waveNumber);

		// Payout reward
		if (game.groupId != 0)
		{
			CPlayerGroup* group = CPlayerGroup::FromGroupID(game.groupId);
			group->RewardMembers(wave.reward);
		}
		else
		{
			auto player = Hk::Client::GetCharacterNameByID(game.members.front());
			Hk::Player::AddCash(player.value(), wave.reward);
		}

		// Send Voice Line
		SendComm(game.members, wave.endVoiceLine);

		// Increment Wave
		game.waveNumber++;

		// Message each player telling them the wave is over. Do after wave number increment because we want wave 1 not 0.
		for (auto const& player : game.members)
			PrintUserCmdText(player, std::format(L"Wave {} complete. Reward: {} credits.", game.waveNumber, wave.reward));

		// Is there a next wave? If so start it on a timer. If not, EndSurvival()
		if (game.waveNumber >= game.system.waves.size())
			EndGame(game, true);
		else
			global->systemsPendingNewWave.push_back(game.system.systemId);
	}

	/** @ingroup WaveDefence
	 * @brief Starts the next wave on a timer. This is so the wave doesnt start immediately after the end of the previous one
	 */
	void WaveTimer()
	{
		for (const auto& systemId : global->systemsPendingNewWave)
		{
			auto game = std::ranges::find_if(global->games.begin(), global->games.end(), [systemId](auto& item) { return item.system.systemId == systemId; });
			NewWave(*game);
		}
		global->systemsPendingNewWave.clear();
	}

	/** @ingroup WaveDefence
	 * @brief Starts any pending games that haven't started already
	 */
	void GameTimer()
	{
		for (auto& game : global->games)
		{
			if (!game.started)
			{
				for (const auto& player : game.members)
				{
					// Beam the players to a point in the system
					Matrix rotation = {0.0f, 0.0f, 0.0f};
					Hk::Player::RelocateClient(player, game.system.positionVector, rotation);
				}
				game.started = true;
				NewWave(game);
			}
		}
	}

	const std::vector<Timer> timers = {{WaveTimer, 5}, {GameTimer, 5}};

	/** @ingroup WaveDefence
	 * @brief Hook on BaseDestroyed. Checks if the base was part of a game.
	 */
	void BaseDestroyed(uint objectId, [[maybe_unused]] uint clientBy)
	{
		for (auto& game : global->games)
		{
			// Remove Solar if part of a wave
			auto solarSubRange = std::ranges::remove_if(game.spawnedSolars, [objectId](auto& item) { return item == objectId; });
			game.spawnedSolars.erase(solarSubRange.begin(), solarSubRange.end());

			// If there's no more NPCs or Solars, end of the wave
			if (game.spawnedNpcs.empty() && game.spawnedSolars.empty())
				EndWave(game);
		}
	}

	/** @ingroup WaveDefence
	 * @brief Hook on ShipDestroyed. Checks if the ship was part of a game.
	 */

	// TO DO merge the spawnedSolars and spawnedNpcs list.

	void ShipDestroyed([[maybe_unused]] DamageList** _dmg, const DWORD** ecx, [[maybe_unused]] const uint& kill)
	{
		// Grab the ship from the ecx
		const CShip* ship = Hk::Player::CShipFromShipDestroyed(ecx);
		
		// Skip the for loop if its a player
		if (ship->is_player())
		{
			return;
		}

		for (auto& game : global->games)
		{
			// Remove NPC if part of a wave
			auto npcSubRange = std::ranges::remove_if(game.spawnedNpcs, [ship](auto& item) { return item == ship->get_id(); });
			game.spawnedNpcs.erase(npcSubRange.begin(), npcSubRange.end());

			// If there's no more NPCs or Solars, end of the wave
			if (game.spawnedNpcs.empty() && game.spawnedSolars.empty())
				EndWave(game);
		}
	}

	/** @ingroup WaveDefence
	 * @brief Remove the client from any games.
	 */
	void Disqualify(uint client)
	{
		// Are they even in a game?
		for (auto& game : global->games)
		{
			if (auto it = std::ranges::find(game.members, client); it != game.members.end())
			{
				// Remove from group
				if (game.groupId != 0)
				{
					CPlayerGroup* group = CPlayerGroup::FromGroupID(game.groupId);
					group->DelMember(client); // Remove from group
				}

				// Remove from member list
				game.members.erase(it);

				// Mission Failed.
				ShowPlayerMissionText(client, 13085, MissionMessageType_Failure);

				// Play loser music
				pub::Audio::SetMusic(client, global->config->failureMusicId);

				// Any members left?
				if (game.members.empty())
				{
					EndGame(game, false);
					return;
				}

				// Send fleeing message
				auto playerCharacterName = Hk::Client::GetCharacterNameByID(client);
				for (auto const& player : game.members)
					PrintUserCmdText(player, std::vformat(global->config->fleeingMessage, std::make_wformat_args(playerCharacterName.value())));
			}
		}
	}

	// Disqualify from survival if these hooks go off
	void __stdcall DisConnect(unsigned int client, [[maybe_unused]] enum EFLConnection state)
	{
		Disqualify(client);
	}

	void __stdcall BaseEnter([[maybe_unused]] unsigned int base, unsigned int client)
	{
		Disqualify(client);
	}

	void __stdcall PlayerLaunch([[maybe_unused]] unsigned int ship, unsigned int client)
	{
		Disqualify(client);
	}

	const std::vector commands = {
	    {CreateUserCommand(L"/wave", L"", NewGame, L"Starts a wave defence game.")
	}};
} // namespace Plugins::WaveDefence

using namespace Plugins::WaveDefence;

DefaultDllMainSettings(LoadSettings);

REFL_AUTO(type(CostumeStrings), field(body), field(head), field(lefthand), field(righthand), field(accessory));
REFL_AUTO(type(Character), field(voice), field(infocard), field(costumeStrings));
REFL_AUTO(type(VoiceLine), field(voiceLineString), field(character));
REFL_AUTO(type(Wave), field(npcs), field(variableNpcs), field(reward), field(startVoiceLine), field(endVoiceLine), field(solars));
REFL_AUTO(type(System), field(system), field(waves), field(posX), field(posY), field(posZ));
REFL_AUTO(type(Config), field(systems), field(characters), field(victoryMusic), field(failureMusic), field(npcMultiplier));

extern "C" EXPORT void ExportPluginInfo(PluginInfo* pi)
{
	pi->name("Wave Defence");
	pi->shortName("wave_defence");
	pi->mayUnload(true);
	pi->returnCode(&global->returnCode);
	pi->timers(&timers);
	pi->commands(&commands);
	pi->versionMajor(PluginMajorVersion::VERSION_04);
	pi->versionMinor(PluginMinorVersion::VERSION_00);
	pi->emplaceHook(HookedCall::FLHook__LoadSettings, &LoadSettings, HookStep::After);
	pi->emplaceHook(HookedCall::IServerImpl__BaseEnter, &BaseEnter);
	pi->emplaceHook(HookedCall::IServerImpl__DisConnect, &DisConnect);
	pi->emplaceHook(HookedCall::IServerImpl__PlayerLaunch, &PlayerLaunch);
	pi->emplaceHook(HookedCall::IEngine__BaseDestroyed, &BaseDestroyed);
	pi->emplaceHook(HookedCall::IEngine__ShipDestroyed, &ShipDestroyed);

	global->npcCommunicator = static_cast<Plugins::Npc::NpcCommunicator*>(PluginCommunicator::ImportPluginCommunicator(Plugins::Npc::NpcCommunicator::pluginName));
	global->solarCommunicator = static_cast<Plugins::SolarControl::SolarCommunicator*>(
	    PluginCommunicator::ImportPluginCommunicator(Plugins::SolarControl::SolarCommunicator::pluginName));
}
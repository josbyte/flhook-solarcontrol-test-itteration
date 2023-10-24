/**
 * @date Unknown
 * @author JosByte
 * @defgroup JosAnimations Animations Animation
 * @brief
 * Framework focused on play animations at a certain place, with other multiple stuff.
 *
 * @paragraph cmds Player Commands
 *
 * There are no player commands in this plugin.
 *
 * @paragraph adminCmds Admin Commands
 * There are no admin commands in this plugin.
 *
 * @endcode
 *
 * @paragraph ipc IPC Interfaces Exposed
 * This plugin does not expose any functionality.
 *
 * @paragraph optional Optional Plugin Dependencies
 * None
 */


#include "JosAnimations.h"

namespace Plugins::JosAnimations
{
	const std::unique_ptr<Global> global = std::make_unique<Global>();


	void generateAnimation(Vector origin, auto animationName, uint iSystem)
	{
		//Origin is where the animation will appear


		int animationObject = generateAnimationSolar(origin, animationName, iSystem);

	}

	void generateAnimationSolar(Vector origin, auto animationName, const Matrix& rot, uint iSystem)
	{
		Hk::Message::FMsgU(L"inside generation");
		std::string pluginName = "JosAnimations"; 

		// Check IPC is working
		if (!global->solarCommunicator)
		{
			Hk::Message::FMsgU(L"There has been an error with the anim plugin. Please contact an administrator.");
			return;
		}

		global->solarCommunicator->CreateSolar(animationName, origin, rot, iSystem, true, false);

	}

	void UserCommandHandler(ClientId& client, const std::wstring& param)
	{
		const auto currentSystem = Hk::Player::GetSystem(client);
		if (currentSystem.has_error())
		{
			PrintUserCmdText(client, L"Unable to decipher player location.");
			return;
		}

		if (const auto cmd = GetParam(param, L' ', 0); cmd == L"generateSolarTest")
		{
			uint ship = Hk::Player::GetShip(client).value();
			SystemId iClientSystemId = Hk::Player::GetSystem(client).value();

			Vector pos {};
			Matrix rot {};
			pub::SpaceObj::GetLocation(ship, pos, rot);

			global->solarCommunicator->CreateSolar(L"kus_proxysensor_nab", pos, rot, iClientSystemId, true, false);
		}

	}

	const std::vector commands = {CreateUserCommand(L"/testjos", L"", UserCommandHandler, L"Accepts the current duel request.")};

	

	// Client command processing



} // namespace Plugins::JosAnimations


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FLHOOK STUFF
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace Plugins::JosAnimations;

extern "C" EXPORT void ExportPluginInfo(PluginInfo* pi)
{
	pi->name("Jos Animations");
	pi->shortName("josanimations");
	pi->mayUnload(true);
	pi->commands(&commands);
	pi->returnCode(&global->returnCode);
	pi->versionMajor(PluginMajorVersion::VERSION_04);
	pi->versionMinor(PluginMinorVersion::VERSION_00);
	global->solarCommunicator = static_cast<Plugins::SolarControl::SolarCommunicator*>(
	    PluginCommunicator::ImportPluginCommunicator(Plugins::SolarControl::SolarCommunicator::pluginName));
}
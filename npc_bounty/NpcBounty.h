#pragma once

// Included
#include <FLHook.hpp>
#include <plugin.h>

namespace Plugins::NpcBounty
{

	//! Configurable fields for this plugin
	struct Config : Reflectable
	{
		std::string File() override { return "config/npc_bounty_hunt.json"; }
		// Reflectable fields
		bool enableBountyHunt = true;
	};

	//! Global data for this plugin
	struct Global final
	{
		std::unique_ptr<Config> config = nullptr;
		ReturnCode returnCode = ReturnCode::Default;
	};
} // namespace Plugins::NpcBountyHunt
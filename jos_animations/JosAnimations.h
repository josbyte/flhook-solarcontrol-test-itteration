#pragma once

// Included
#include <FLHook.hpp>
#include <plugin.h>
#include "../solar_control/SolarControl.h"

namespace Plugins::JosAnimations
{

	//! Configurable fields for this plugin
	struct Config : Reflectable
	{
		// Reflectable fields

	};

	//! Global data for this plugin
	struct Global final
	{
		std::unique_ptr<Config> config = nullptr;
		Plugins::SolarControl::SolarCommunicator* solarCommunicator = nullptr;
		ReturnCode returnCode = ReturnCode::Default;
	};
} // namespace Plugins::JosAnimations
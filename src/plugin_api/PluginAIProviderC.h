#pragma once

// Compatibility header.
// Some older code paths referenced an AI-provider specific header name.
// The AI provider plugin uses the general C-ABI plugin API headers.

#include "PluginC.h"
#include "PluginTypesC.h"
#include "PluginEventsC.h"
#include "PluginLoggerC.h"
#include "PluginKeyEncryptionC.h"

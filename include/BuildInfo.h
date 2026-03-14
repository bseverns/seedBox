#pragma once

#if defined(__has_include)
#if __has_include("BuildInfo.generated.h")
#include "BuildInfo.generated.h"
#endif
#endif

#ifndef SEEDBOX_GIT
#ifdef SEEDBOX_VERSION
#define SEEDBOX_GIT SEEDBOX_VERSION
#else
#define SEEDBOX_GIT "dev"
#endif
#endif

#ifndef SEEDBOX_BUILT
#define SEEDBOX_BUILT "unknown"
#endif

#pragma once

// Build metadata stamped in at compile time.  Great for showing students how to
// surface git SHA + build timestamps on a microcontroller without blowing flash
// budgets.
#ifndef SEEDBOX_GIT
#define SEEDBOX_GIT "nogit"
#endif
#ifndef SEEDBOX_BUILT
#define SEEDBOX_BUILT "unknown"
#endif

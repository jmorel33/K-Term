/***************************************************************************************************
*
*   -- The "Situation" Advanced Platform Awareness, Control, and Timing --
*   Core API library (see version in Version Macros)
*   (c) 2025 Jacques Morel
*   MIT Licensed
*
*   SPLIT HEADER ARCHITECTURE (v2.3.41+)
*   ====================================
*   This file now acts as a bridge that includes:
*   - situation_api.h   : Public API declarations, types, enums, function prototypes
*   - situation_impl.h  : Implementation section (only compiled when SITUATION_IMPLEMENTATION is defined)
*
*   This split improves code organization and allows for cleaner separation of concerns.
*   Users should include this file (situation.h) as before - the split is transparent.
*
***************************************************************************************************/

#ifndef SITUATION_H
#define SITUATION_H

// Include the public API declarations
#include "situation_api.h"

// Include the implementation section (if requested)
#ifdef SITUATION_IMPLEMENTATION
    #include "situation_impl.h"
#endif

#endif // SITUATION_H

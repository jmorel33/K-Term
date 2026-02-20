// kterm.h - K-Term Terminal Emulation Library
// Wrapper header for backward compatibility
// This includes both the API and implementation

#ifndef KTERM_H_WRAPPER
#define KTERM_H_WRAPPER

// Include font data implementation
#define FONT_DATA_IMPLEMENTATION
#include "font_data.h"
#undef FONT_DATA_IMPLEMENTATION

// Include API
#include "kterm_api.h"

#ifndef KTERM_DISABLE_VOICE
#include "kt_voice.h"
#endif

#ifndef KTERM_DISABLE_VOIP
#include "kt_voip.h"
#endif

// Include implementation if requested
#ifdef KTERM_IMPLEMENTATION
#include "kt_parser.h"
#define KTERM_LAYOUT_IMPLEMENTATION
#include "kt_layout.h"

#include "kterm_impl.h"

#define KTERM_VOICE_IMPLEMENTATION
#ifndef KTERM_DISABLE_VOICE
#include "kt_voice.h"
#endif

#define KTERM_VOIP_IMPLEMENTATION
#ifndef KTERM_DISABLE_VOIP
#include "kt_voip.h"
#endif

#ifdef KTERM_ENABLE_GATEWAY
#define KTERM_GATEWAY_IMPLEMENTATION
#include "kt_gateway.h"
#undef KTERM_GATEWAY_IMPLEMENTATION
#endif

#define KTERM_NET_IMPLEMENTATION
#ifndef KTERM_DISABLE_NET
#include "kt_net.h"
#endif

#define KTERM_COMPOSITE_IMPLEMENTATION
#include "kt_composite_sit.h"
#undef KTERM_COMPOSITE_IMPLEMENTATION
#endif

#endif // KTERM_H_WRAPPER

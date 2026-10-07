#pragma once

#if defined(ROBOTPAL_TRACY_ENABLED)
    #include <tracy/Tracy.hpp>
    #define RP_PROFILE_SCOPE(name) ZoneScopedN(name)
    #define RP_PROFILE_FRAME() FrameMark
    #define RP_PROFILE_THREAD(name) tracy::SetThreadName(name)
#else
    #define RP_PROFILE_SCOPE(name) ((void)0)
    #define RP_PROFILE_FRAME() ((void)0)
    #define RP_PROFILE_THREAD(name) ((void)0)
#endif

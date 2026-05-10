// pch.h: This is a pre-compiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

#include <WinSock2.h>     
#include <WS2tcpip.h> 

#include "framework.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#endif //PCH_H
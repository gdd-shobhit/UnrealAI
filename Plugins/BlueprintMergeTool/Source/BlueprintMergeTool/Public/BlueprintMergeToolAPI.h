#pragma once

#include "CoreMinimal.h"

#ifndef BLUEPRINTMERGETOOL_API
	#if defined(BLUEPRINTMERGETOOL_EXPORTS)
		#define BLUEPRINTMERGETOOL_API __declspec(dllexport)
	#else
		#define BLUEPRINTMERGETOOL_API __declspec(dllimport)
	#endif
#endif

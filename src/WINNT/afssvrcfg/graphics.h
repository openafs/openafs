/*
 * Copyright (C) 1998  Transarc Corporation.
 * All rights reserved.
 *
 */

#ifndef	_GRAPHICS_H_
#define _GRAPHICS_H_

#include "config.h"

void PaintStepGraphic(HWND hwnd, STEP_STATE state);

void CALLBACK PaintPageGraphic(LPWIZARD pWiz, HDC hdcTarget, LPRECT prTarget, HPALETTE hpal);

#endif	// _GRAPHICS_H_


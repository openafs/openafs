/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_GRAPHICS_H_
#define _GRAPHICS_H_

#include "config.h"

void PaintStepGraphic(HWND hwnd, STEP_STATE state);

void CALLBACK PaintPageGraphic(LPWIZARD pWiz, HDC hdcTarget, LPRECT prTarget, HPALETTE hpal);

#endif	// _GRAPHICS_H_


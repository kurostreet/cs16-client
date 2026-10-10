/*
CzmMenu.h - Counter Zombie Mobile main menu art and loading screen
Copyright (C) 2026 Counter Zombie Mobile authors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#pragma once
#ifndef CZM_MENU_H
#define CZM_MENU_H

#include <stddef.h>
#include "Primitive.h"

// draws the full-screen art behind every menu window, returns false if the art is missing
bool CZM_DrawMenuBackground( Point pos, Size size );

// draws the loading screen while the engine connects, returns true if it owns the frame
bool CZM_DrawLoadingScreen( void );

// fonts are recreated together with the menu fonts when the resolution changes
void CZM_CreateFonts( float scale );
bool CZM_FindFontFile( const char *name, char *dataFile, size_t dataFileChars );

#endif // CZM_MENU_H

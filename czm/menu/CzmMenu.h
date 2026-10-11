/*
CzmMenu.h - Counter Zombie Mobile menu: shared drawing, language, servers, loading screen
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
#include "BaseMenu.h"

/*
 * fonts
 */
enum CzmFontId
{
	CZMF_LOAD_HOST = 0,
	CZMF_LOAD_PERCENT,
	CZMF_LOAD_INFO,
	CZMF_LOAD_STATUS,
	CZMF_LOAD_TIP,
	CZMF_BUTTON,
	CZMF_TILE_TITLE,
	CZMF_TILE_TEXT,
	CZMF_TILE_STATUS,
	CZMF_NICK,
	CZMF_SMALL,
	CZMF_NAV,
	CZMF_LANG,
	CZMF_PANEL_TITLE,
	CZMF_PANEL_TEXT,
	CZMF_COUNT
};

HFont CZM_Font( int id );
int CZM_FontHeight( HFont font );
int CZM_TextWidth( HFont font, const char *text );
// single line, cut with an ellipsis when it does not fit into w
void CZM_DrawText( HFont font, int x, int y, int w, const char *text, unsigned int color, unsigned int justify = QM_LEFT );

// fonts are recreated together with the menu fonts when the resolution changes
void CZM_CreateFonts( float scale );
bool CZM_FindFontFile( const char *name, char *dataFile, size_t dataFileChars );

/*
 * images (loaded once, by path)
 */
HIMAGE CZM_Image( const char *path );
void CZM_DrawImage( const char *path, int x, int y, int w, int h, unsigned int color = 0xFFFFFFFF, bool transparent = true );
// stretches a rounded image, the corners keep their shape
void CZM_DrawSliced( const char *path, int x, int y, int w, int h, int imageBorder, int screenBorder, unsigned int color = 0xFFFFFFFF );
// pill-shaped image: the round ends keep their shape, the middle stretches
void CZM_DrawPill( const char *path, int x, int y, int w, int h, unsigned int color = 0xFFFFFFFF );
int CZM_ImageAspect1000( const char *path ); // width * 1000 / height, 0 if missing
void CZM_DrawCover( HIMAGE image, int x, int y, int w, int h );

/*
 * misc
 */
void CZM_Init( void ); // registers our cvars, safe to call often
unsigned int CZM_PingColor( int ping );
void CZM_CleanName( const char *in, char *out, size_t size ); // drops colour codes
bool CZM_CursorIn( int x, int y, int w, int h );
const char *CZM_Version( void );
void CZM_OpenConsole( void ); // hidden debug entry

/*
 * language: Russian or English, the default follows the phone language
 */
const char *CZM_T( const char *key );
bool CZM_IsRussian( void );
void CZM_SetLanguage( bool russian );

/*
 * our servers (the game has no public server list)
 */
struct CzmServer
{
	const char *title;    // mode name shown on the tile
	const char *address;  // ip:port
	const char *tile;     // tile picture
	unsigned int accent;  // tile colour
	char map[64];
	int players;
	int maxplayers;
	int ping;             // milliseconds, -1 while unknown
	double queryTime;     // last query sent
	double answerTime;    // last info received
	int misses;           // queries in a row without an answer
};

int CZM_ServerCount( void );
CzmServer *CZM_Server( int index );
void CZM_QueryServers( bool force );
bool CZM_OnServerInfo( netadr_t adr, const char *info ); // true for our own messages, which the stock list must not see
void CZM_ConnectToServer( int index );

/*
 * screens
 */
// draws the full-screen art behind every menu window, returns false if the art is missing
bool CZM_DrawMenuBackground( Point pos, Size size );

// draws the loading screen while the engine connects, returns true if it owns the frame
bool CZM_DrawLoadingScreen( void );

#endif // CZM_MENU_H

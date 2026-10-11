/*
CzmServerMenu.cpp - Counter Zombie Mobile: server menus as big cards over the game
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extdll_menu.h"
#include "BaseMenu.h"
#include "Utils.h"
#include "CzmMenu.h"

#define UI_DIR "gfx/czm/ui/"
#define MAX_CARDS 10
#define MAX_LINES 3

/*
Menus from plugins, votes and the classic buy menu come as text: a title, then lines
like "\r1. \wItem" with colour codes (\y \r \w \d, \R moves the rest to the right).
The client hands the text over in the czm_menu cvar ("serial validslots", a new line,
the text). Each item becomes a card; invisible client touch buttons over the cards
send slot1..slot10, so the rest of the screen keeps working: the player can walk
and look around while the menu is open.
*/
struct CzmCard
{
	int key;          // 1..10 (10 is key 0)
	char text[96];
	char right[48];
	bool enabled;
	CzmRect rect;
};

static struct
{
	int serial;       // the menu on the screen, -1 none
	int validSlots;
	char title[96];
	char lines[MAX_LINES][96];
	int numLines;
	CzmCard cards[MAX_CARDS];
	int numCards;
	CzmRect panel;
	bool buttons;     // touch buttons are in place
	int layoutW, layoutH;
} s_menu = { -1 };

static void TrimCopy( char *out, size_t size, const char *from, const char *to )
{
	while( from < to && ( *from == ' ' || *from == '\t' ))
		from++;
	while( to > from && ( to[-1] == ' ' || to[-1] == '\t' || to[-1] == '\r' ))
		to--;

	size_t len = Q_min( (size_t)( to - from ), size - 1 );
	memcpy( out, from, len );
	out[len] = 0;
}

// one line of the menu: visible text without colour codes, the part after \R, and the colour of the first letter
static void SplitLine( const char *line, char *text, size_t textSize, char *right, size_t rightSize, char *firstColor )
{
	char buf[256], rbuf[128];
	int n = 0, rn = 0;
	bool toRight = false;
	char color = 'w';

	*firstColor = 0;
	for( const char *p = line; *p && *p != '\n'; p++ )
	{
		if( p[0] == '\\' && p[1] && strchr( "yrwdR", p[1] ))
		{
			if( p[1] == 'R' )
				toRight = true;
			else
				color = p[1];
			p++;
			continue;
		}

		if( !toRight )
		{
			if( !*firstColor && *p != ' ' )
				*firstColor = color;
			if( n < (int)sizeof( buf ) - 1 )
				buf[n++] = *p;
		}
		else if( rn < (int)sizeof( rbuf ) - 1 )
			rbuf[rn++] = *p;
	}

	TrimCopy( text, textSize, buf, buf + n );
	TrimCopy( right, rightSize, rbuf, rbuf + rn );
}

static void ParseMenu( const char *value )
{
	const char *text = strchr( value, '\n' );
	const char *line;
	bool seenItem = false;

	s_menu.title[0] = 0;
	s_menu.numLines = 0;
	s_menu.numCards = 0;
	s_menu.validSlots = 0;
	sscanf( value, "%d %d", &s_menu.serial, &s_menu.validSlots );

	if( !text )
		return;

	for( line = text + 1; *line; )
	{
		const char *end = strchr( line, '\n' );
		char visible[128], right[48], color;

		SplitLine( line, visible, sizeof( visible ), right, sizeof( right ), &color );
		line = end ? end + 1 : line + strlen( line );

		if( !visible[0] )
			continue;

		// "1. Item", "1) Item"
		if( visible[0] >= '0' && visible[0] <= '9' && ( visible[1] == '.' || visible[1] == ')' ) && s_menu.numCards < MAX_CARDS )
		{
			CzmCard &card = s_menu.cards[s_menu.numCards++];
			int key = visible[0] == '0' ? 10 : visible[0] - '0';

			card.key = key;
			TrimCopy( card.text, sizeof( card.text ), visible + 2, visible + strlen( visible ));
			Q_strncpy( card.right, right, sizeof( card.right ));
			card.enabled = ( s_menu.validSlots & ( 1 << ( key - 1 ))) && color != 'd';
			seenItem = true;
			continue;
		}

		if( !seenItem && !s_menu.title[0] )
			Q_strncpy( s_menu.title, visible, sizeof( s_menu.title ));
		else if( s_menu.numLines < MAX_LINES )
			Q_strncpy( s_menu.lines[s_menu.numLines++], visible, sizeof( s_menu.lines[0] ));
	}
}

static void RemoveButtons( void )
{
	if( !s_menu.buttons )
		return;

	EngFuncs::ClientCmd( false, "touch_removebutton \"_menu_czm*\"\n" );
	s_menu.buttons = false;
}

static void Layout( void )
{
	int W = ScreenWidth, H = ScreenHeight;
	int columns = s_menu.numCards > 6 ? 2 : 1;
	int perColumn = ( s_menu.numCards + columns - 1 ) / columns;
	int x = W * 0.025f, top = H * 0.16f;
	int colW = columns > 1 ? Q_min( (int)( W * 0.22f ), (int)( H * 0.50f )) : Q_min( (int)( W * 0.30f ), (int)( H * 0.66f ));
	int cardH = H * 0.074f, gap = H * 0.012f;
	int y;

	// title, extra lines, then the cards
	y = top + CZM_FontHeight( CZM_Font( CZMF_SMALL )) + CZM_FontHeight( CZM_Font( CZMF_MENU_TITLE )) + H * 0.02f;
	y += s_menu.numLines * ( CZM_FontHeight( CZM_Font( CZMF_SMALL )) + H * 0.006f );

	for( int i = 0; i < s_menu.numCards; i++ )
	{
		int col = i / perColumn, row = i % perColumn;
		s_menu.cards[i].rect = CZM_Rect( x + col * ( colW + gap ), y + row * ( cardH + gap ), colW, cardH );
	}

	s_menu.panel = CZM_Rect( 0, 0, x + columns * colW + ( columns - 1 ) * gap + W * 0.03f, H );
	s_menu.layoutW = W;
	s_menu.layoutH = H;
}

static void AddButtons( void )
{
	float W = ScreenWidth, H = ScreenHeight;

	RemoveButtons();
	for( int i = 0; i < s_menu.numCards; i++ )
	{
		const CzmCard &card = s_menu.cards[i];

		if( !card.enabled )
			continue;

		// client button (flag 4): not saved, removed with the other menu buttons
		EngFuncs::ClientCmdF( false, "touch_addbutton \"_menu_czm%d\" \"\" \"slot%d\" %f %f %f %f 255 255 255 0 4\n",
			card.key, card.key, card.rect.x / W, card.rect.y / H, ( card.rect.x + card.rect.w ) / W, ( card.rect.y + card.rect.h ) / H );
	}
	s_menu.buttons = true;
}

static void DrawMenu( void )
{
	int H = ScreenHeight;
	HFont labelFont = CZM_Font( CZMF_SMALL );
	HFont titleFont = CZM_Font( CZMF_MENU_TITLE );
	HFont textFont = CZM_Font( CZMF_LANG );
	HFont rightFont = CZM_Font( CZMF_TILE_STATUS );
	HFont keyFont = CZM_Font( CZMF_SHOP_PRICE );
	int x = ScreenWidth * 0.025f, y = H * 0.16f, w = s_menu.panel.w - x * 2;
	int corner = H * 0.022f;

	// a dark fade behind the menu keeps it readable over any map
	CZM_DrawImage( "gfx/czm/fade_left.png", 0, 0, s_menu.panel.w + ScreenWidth * 0.08f, H, CZM_White( 210 ));

	CZM_DrawText( labelFont, x, y, w, CZM_IsRussian() ? "МЕНЮ СЕРВЕРА" : "SERVER MENU", PackRGBA( 154, 163, 175, 255 ));
	y += CZM_FontHeight( labelFont );
	if( s_menu.title[0] )
		CZM_DrawText( titleFont, x, y, w, s_menu.title, CZM_White());
	y += CZM_FontHeight( titleFont ) + H * 0.01f;

	for( int i = 0; i < s_menu.numLines; i++ )
	{
		CZM_DrawText( labelFont, x, y, w, s_menu.lines[i], PackRGBA( 203, 210, 220, 255 ));
		y += CZM_FontHeight( labelFont ) + H * 0.006f;
	}

	for( int i = 0; i < s_menu.numCards; i++ )
	{
		const CzmCard &card = s_menu.cards[i];
		const CzmRect &r = card.rect;
		int badge = r.h * 0.62f, pad = ( r.h - badge ) / 2;
		int textX = r.x + pad * 2 + badge;
		int rightW = card.right[0] ? CZM_TextWidth( rightFont, card.right ) + pad : 0;
		char key[4];

		CZM_DrawSliced( UI_DIR "card.png", r.x, r.y, r.w, r.h, 30, corner, CZM_White( card.enabled ? 245 : 140 ));
		CZM_DrawImage( UI_DIR "dot.png", r.x + pad, r.y + pad, badge, badge,
			card.enabled ? PackRGBA( 220, 38, 38, 255 ) : PackRGBA( 63, 70, 82, 255 ));
		snprintf( key, sizeof( key ), "%d", card.key % 10 );
		CZM_DrawText( keyFont, r.x + pad, r.y + ( r.h - CZM_FontHeight( keyFont )) / 2, badge, key, CZM_White(), QM_CENTER );

		CZM_DrawText( textFont, textX, r.y + ( r.h - CZM_FontHeight( textFont )) / 2, r.x + r.w - textX - pad - rightW, card.text,
			card.enabled ? CZM_White() : PackRGBA( 124, 133, 146, 255 ));
		if( card.right[0] )
			CZM_DrawText( rightFont, r.x + r.w - pad - rightW, r.y + ( r.h - CZM_FontHeight( rightFont )) / 2, rightW, card.right,
				PackRGBA( 154, 163, 175, 255 ), QM_RIGHT );
	}
}

void CZM_DrawGameOverlay( void )
{
	const char *value = EngFuncs::GetCvarString( "czm_menu" );
	int serial = -1;
	bool show = value && value[0] && CL_IsActive() && !uiStatic.menu.IsActive() && !uiStatic.client.IsActive()
		&& EngFuncs::GetCvarFloat( "czm_menu_cards" ) != 0.0f;

	if( !show )
	{
		RemoveButtons();
		if( !value || !value[0] )
			s_menu.serial = -1;
		return;
	}

	sscanf( value, "%d", &serial );
	if( serial != s_menu.serial )
	{
		ParseMenu( value );
		Layout();
		AddButtons();
	}
	else if( s_menu.layoutW != ScreenWidth || s_menu.layoutH != ScreenHeight || !s_menu.buttons )
	{
		Layout();
		AddButtons();
	}

	DrawMenu();
}

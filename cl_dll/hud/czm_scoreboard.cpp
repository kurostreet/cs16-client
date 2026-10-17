/* Counter Zombie Mobile player list. GPL-2.0-or-later. */
#include "hud.h"
#include "cl_util.h"
#include "draw_util.h"
#include "czm_avatars.h"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int xstart, xend, ystart, yend;

namespace {
bool Russian()
{
	const char *lang = gEngfuncs.pfnGetCvarString( "czm_lang" );
	if( !lang || !*lang ) lang = getenv( "CZM_LOCALE" );
	return !lang || !*lang || !strncmp( lang, "ru", 2 ) || !strncmp( lang, "uk", 2 )
		|| !strncmp( lang, "be", 2 ) || !strncmp( lang, "kk", 2 );
}

// Use the console's UTF-8 font instead of the legacy credits-font atlas.
// Strip server/player colour escapes so one row cannot change other columns.
void Clean( const char *src, char *out, size_t size )
{
	size_t n = 0;
	if( !src ) src = "";
	for( size_t i = 0; src[i] && n + 1 < size; i++ )
	{
		unsigned char c = (unsigned char)src[i];
		if( c == '^' && src[i + 1] >= '0' && src[i + 1] <= '9' ) { i++; continue; }
		if( c == '\\' && src[i + 1] && strchr( "yrwdR", src[i + 1] )) { i++; continue; }
		if( c < 32 || c == 127 ) continue;
		out[n++] = c;
	}
	out[n] = 0;
	// Drop a partial UTF-8 sequence at the buffer boundary.
	if( *src && n + 1 == size )
	{
		size_t end = n;
		while( end && ((unsigned char)out[end - 1] & 0xc0) == 0x80 ) end--;
		if( end && (unsigned char)out[end - 1] >= 0xc0 ) out[end - 1] = 0;
	}
}

int Width( const char *s )
{
	int w = 0, h = 0;
	gEngfuncs.pfnDrawConsoleStringLen( s, &w, &h );
	return w;
}

void Text( int x, int y, int width, const char *s, int r = 225, int g = 230, int b = 238, bool right = false )
{
	if( width <= 0 ) return;
	char text[256];
	Clean( s, text, sizeof( text ));
	bool cut = false;
	while( *text && Width( text ) > width )
	{
		size_t end = strlen( text ) - 1;
		while( end && ((unsigned char)text[end] & 0xc0) == 0x80 ) end--;
		text[end] = 0;
		cut = true;
	}
	if( cut )
	{
		while( *text && Width( text ) + Width( "..." ) > width )
		{
			size_t end = strlen( text ) - 1;
			while( end && ((unsigned char)text[end] & 0xc0) == 0x80 ) end--;
			text[end] = 0;
		}
		if( strlen( text ) + 3 < sizeof( text ) && Width( "..." ) <= width ) strcat( text, "..." );
	}
	gEngfuncs.pfnDrawSetTextColor( r / 255.0f, g / 255.0f, b / 255.0f );
	gEngfuncs.pfnDrawConsoleString( right ? x + width - Width( text ) : x, y, text );
}

int Team( int id )
{
	int team = g_PlayerExtraInfo[id].teamnumber;
	return team == 1 || team == 2 ? team : 3;
}
}

int CHudScoreboard::DrawCzmScoreboard( void )
{
	GetAllPlayersInfo();
	int players[MAX_PLAYERS], count = 0;
	for( int i = 1; i <= MAX_PLAYERS; i++ )
		if( g_PlayerInfoList[i].name && g_PlayerInfoList[i].name[0] ) players[count++] = i;
	std::sort( players, players + count, []( int a, int b ) {
		if( Team( a ) != Team( b )) return Team( a ) < Team( b );
		if( g_PlayerExtraInfo[a].frags != g_PlayerExtraInfo[b].frags )
			return g_PlayerExtraInfo[a].frags > g_PlayerExtraInfo[b].frags;
		if( g_PlayerExtraInfo[a].deaths != g_PlayerExtraInfo[b].deaths )
			return g_PlayerExtraInfo[a].deaths < g_PlayerExtraInfo[b].deaths;
		return a < b;
	});
	const bool ru = Russian();
	const float requested = gEngfuncs.pfnGetCvarFloat( "czm_tab_style" );
	const int style = requested >= 2 ? 2 : requested >= 1 ? 1 : 0;
	int x = xstart, y0 = ystart, w = xend - xstart, h = yend - ystart;
	if( w <= 0 || h <= 0 ) return 1;
	const int pad = std::max( 6, h / 40 ), gap = pad * ( style == 2 ? 3 : 2 );
	int tw = 0, fontH = 0;
	gEngfuncs.pfnDrawConsoleStringLen( "Ag", &tw, &fontH );
	// Console metrics and all scoreboard coordinates are already in HUD units.
	fontH = std::max( 8, fontH );
	const int head = std::max( style == 1 ? 0 : h / 7, fontH * 3 + pad * 3 );
	int columns = style == 1 ? ( count <= 10 ? 1 : 2 ) : ( w > ScreenHeight * 0.9f ? 2 : 1 );
	if( style == 1 )
	{
		int narrow = w * ( columns == 1 ? 0.72f : 0.92f );
		x += ( w - narrow ) / 2; w = narrow;
	}
	// At large text scales use more columns instead of dropping players or overlapping text.
	const int capacity = std::max( 1, ( h - head - pad * 2 - fontH * 2 ) / ( fontH + 1 ));
	while( columns < 4 && count > columns * capacity ) columns++;
	const int rows = std::max( 1, ( count + columns - 1 ) / columns );
	const int cw = ( w - pad * 2 - gap * ( columns - 1 )) / columns;
	const int available = h - head - pad * 2 - fontH * 2;
	const int reserve = style == 0 ? std::max( columns == 1 ? 8 : ( MAX_PLAYERS + columns - 1 ) / columns, rows ) : rows;
	int rh = std::max( 1, available / reserve );
	if( available / rows >= fontH + 1 ) rh = std::max( rh, fontH + 1 );
	if( style != 0 )
	{
		rh = std::min( rh, fontH + ( style == 1 ? std::max( 4, pad / 2 ) : pad * 2 ));
		int fitted = std::min( h, head + rh * rows + pad * 2 + fontH * 2 );
		y0 += ( h - fitted ) / 2; h = fitted;
	}
	if( style != 2 ) FillRGBABlend( x, y0, w, h, 10, 13, 19, style == 1 ? 245 : 235 );
	else FillRGBABlend( x, y0, w, head - fontH - pad * 2, 10, 13, 19, 235 );
	FillRGBA( x, y0, w, std::max( 2, h / 180 ), 224, 35, 47, 255 );
	char buf[256];
	snprintf( buf, sizeof( buf ), "%s  /  %d", ru ? "ИГРОКИ" : "PLAYERS", count );
	Text( x + pad, y0 + pad, w * 2 / 3, buf, 255, 255, 255 );
	Text( x + pad, y0 + pad + fontH + pad / 2, w - pad * 2,
		gHUD.m_szServerName[0] ? gHUD.m_szServerName : "COUNTER ZOMBIE MOBILE", 158, 168, 185 );
	for( int c = 0; c < columns; c++ )
	{
		const int left = x + pad + c * ( cw + gap );
		const int nameW = cw * 0.57f, statW = cw * 0.13f;
		const int hy = y0 + head - fontH - pad;
		if( style == 2 )
		{
			int used = std::max( 0, std::min( rows, count - c * rows ));
			FillRGBABlend( left - pad / 2, hy - pad / 2, cw + pad, fontH + pad * 2 + used * rh, 10, 13, 19, 225 );
			FillRGBA( left - pad / 2, hy - pad / 2, cw + pad, 1, 77, 85, 102, 220 );
		}
		Text( left, hy, nameW, ru ? "ИГРОК / СТАТУС" : "PLAYER / STATUS", 162, 171, 187 );
		Text( left + nameW, hy, statW, ru ? "УБ." : "KILLS", 162, 171, 187, true );
		Text( left + nameW + statW, hy, statW, ru ? "СМ." : "DEATHS", 162, 171, 187, true );
		Text( left + nameW + statW * 2, hy, cw - nameW - statW * 2, "PING", 162, 171, 187, true );
		for( int row = 0; row < rows; row++ )
		{
			int n = c * rows + row;
			if( n >= count ) break;
			const int id = players[n], y = y0 + head + row * rh;
			const hud_player_info_t &p = g_PlayerInfoList[id];
			const extra_player_info_t &extra = g_PlayerExtraInfo[id];
			const int team = Team( id );
			const int space = style == 2 ? std::max( 2, std::min( pad / 2, rh - fontH )) : 1;
			FillRGBABlend( left, y, cw, std::max( 1, rh - space ), p.thisplayer ? 90 : 24, p.thisplayer ? 24 : 28, p.thisplayer ? 34 : 38, p.thisplayer ? 220 : ( row % 2 ? 145 : 205 ));
			FillRGBA( left, y, std::max( 2, pad / 3 ), std::max( 1, rh - space ), team == 1 ? 233 : 75, team == 2 ? 167 : 84, team == 2 ? 245 : 101, 240 );
			int ty = y + std::max( 0, ( rh - fontH ) / 2 );
			const int icon = std::max( 0, std::min( rh - 2, pad * 3 ));
			if( icon > 0 && !CZM_DrawTabAvatar( id, p.thisplayer != 0, left + pad, y + ( rh - icon ) / 2, icon ))
				Text( left + pad, ty, icon, "?", 130, 144, 162 );
			const int nx = left + pad + icon;
			Text( nx, ty, std::max( 0, nameW - pad - icon - pad ), p.name, p.thisplayer ? 255 : 225, p.thisplayer ? 238 : 230, p.thisplayer ? 240 : 238 );
			// Status is conveyed by the stripe and a small live/dead indicator.
			FillRGBA( left + nameW - pad, y + rh / 2, std::max( 2, pad / 2 ), std::max( 2, pad / 2 ), extra.dead ? 239 : 74, extra.dead ? 87 : 212, extra.dead ? 98 : 140, 255 );
			snprintf( buf, sizeof( buf ), "%d", extra.frags );
			Text( left + nameW, ty, statW, buf, 235, 238, 245, true );
			snprintf( buf, sizeof( buf ), "%d", extra.deaths );
			Text( left + nameW + statW, ty, statW, buf, 168, 178, 194, true );
			const char *bot = gEngfuncs.PlayerInfo_ValueForKey ? gEngfuncs.PlayerInfo_ValueForKey( id, "*bot" ) : NULL;
			if( bot && *bot && strcmp( bot, "0" )) snprintf( buf, sizeof( buf ), "BOT" );
			else snprintf( buf, sizeof( buf ), "%d", p.ping );
			Text( left + nameW + statW * 2, ty, cw - nameW - statW * 2, buf, p.ping > 180 ? 248 : 117, p.ping > 180 ? 113 : 212, p.ping > 180 ? 113 : 170, true );
		}
	}
	if( style == 2 ) FillRGBABlend( x, y0 + h - fontH - pad * 2, w, fontH + pad * 2, 10, 13, 19, 225 );
	Text( x + pad, y0 + h - fontH - pad, w - pad * 2,
		ru ? "КРАСНЫЕ / СИНИЕ / НАБЛЮДАТЕЛИ     •     ЗЕЛЁНЫЙ: ЖИВ   КРАСНЫЙ: ПОГИБ" : "RED / BLUE / SPECTATORS     •     GREEN: ALIVE   RED: DEAD", 145, 155, 174 );
	return 1;
}

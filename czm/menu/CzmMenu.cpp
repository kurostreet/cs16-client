/*
CzmMenu.cpp - Counter Zombie Mobile main menu art and loading screen
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

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "extdll_menu.h"
#include "BaseMenu.h"
#include "Utils.h"
#include "BaseWindow.h"
#include "FontManager.h"
#include "CzmMenu.h"

// The engine publishes loading progress through these cvars (engine/client/czm_loading.c).
// Stage numbers must match the engine.
enum
{
	CZM_STAGE_NONE = 0,
	CZM_STAGE_CONNECT,
	CZM_STAGE_HANDSHAKE,
	CZM_STAGE_DOWNLOAD,
	CZM_STAGE_PRECACHE,
	CZM_STAGE_ENTER,
	CZM_STAGE_LOCAL,
};

// cancel button, in screen fractions; the engine uses the same numbers for touch input
#define CZM_CANCEL_X0 0.815f
#define CZM_CANCEL_X1 0.955f
#define CZM_CANCEL_Y0 0.818f
#define CZM_CANCEL_Y1 0.898f

#define CZM_LOADING_IMAGES 5

static HIMAGE s_hMenuArt, s_hTitle, s_hFadeLeft, s_hFadeBottom;
static HIMAGE s_hLoading[CZM_LOADING_IMAGES];
static bool s_bImagesLoaded;

static HFont s_hFontHost, s_hFontPercent, s_hFontInfo, s_hFontStatus, s_hFontTip, s_hFontButton;

static const char *s_szTips[] =
{
	"Совет: зомби быстрее людей — держись ближе к своим и прикрывай спину.",
	"Совет: в узком проходе толпу зомби сдерживать намного проще.",
	"Совет: перезаряжайся заранее, пока зомби далеко.",
	"Совет: не стой на открытом месте — ищи укрытие и высоту.",
	"Совет: дробовик и пулемёт хорошо останавливают зомби вблизи.",
	"Совет: граната помогает отбросить толпу и выиграть пару секунд.",
	"Совет: целься в голову — так зомби падают быстрее.",
	"Совет: стал зомби — нападай вместе с другими, по одному не ходи.",
	"Совет: с друзьями выживать проще — зови их на свой сервер.",
	"Совет: потренируйся с ботами, чтобы привыкнуть к управлению.",
};

/*
=================
images and fonts
=================
*/
static void CZM_LoadImages( void )
{
	if( s_bImagesLoaded )
		return;

	s_bImagesLoaded = true;
	s_hMenuArt = EngFuncs::PIC_Load( "gfx/czm/menu_bg.png" );
	s_hTitle = EngFuncs::PIC_Load( "gfx/czm/title.png" );
	s_hFadeLeft = EngFuncs::PIC_Load( "gfx/czm/fade_left.png" );
	s_hFadeBottom = EngFuncs::PIC_Load( "gfx/czm/fade_bottom.png" );

	for( int i = 0; i < CZM_LOADING_IMAGES; i++ )
	{
		char path[64];
		snprintf( path, sizeof( path ), "gfx/czm/loading_%d.png", i + 1 );
		s_hLoading[i] = EngFuncs::PIC_Load( path );
	}
}

bool CZM_FindFontFile( const char *name, char *dataFile, size_t dataFileChars )
{
	static const struct { const char *name, *file; } fonts[] =
	{
		{ "CZM Black", "gfx/fonts/czm/InterDisplay-Black.otf" },
		{ "CZM Bold", "gfx/fonts/czm/InterDisplay-Bold.otf" },
		{ "CZM Semibold", "gfx/fonts/czm/Inter-SemiBold.otf" },
		{ "CZM Medium", "gfx/fonts/czm/Inter-Medium.otf" },
	};

	for( size_t i = 0; i < sizeof( fonts ) / sizeof( fonts[0] ); i++ )
	{
		if( !strcmp( name, fonts[i].name ))
		{
			Q_strncpy( dataFile, fonts[i].file, dataFileChars );
			return true;
		}
	}

	return false;
}

void CZM_CreateFonts( float scale )
{
	// sizes are in 768-line units, like the rest of the menu
	s_hFontHost = CFontBuilder( "CZM Black", 46 * scale, 900 ).Create();
	s_hFontPercent = CFontBuilder( "CZM Black", 40 * scale, 900 ).Create();
	s_hFontInfo = CFontBuilder( "CZM Semibold", 25 * scale, 600 ).Create();
	s_hFontStatus = CFontBuilder( "CZM Semibold", 23 * scale, 600 ).Create();
	s_hFontTip = CFontBuilder( "CZM Medium", 21 * scale, 500 ).Create();
	s_hFontButton = CFontBuilder( "CZM Semibold", 25 * scale, 600 ).Create();
}

/*
=================
drawing helpers
=================
*/
static void CZM_DrawCover( HIMAGE image, int x, int y, int w, int h )
{
	int iw = image ? EngFuncs::PIC_Width( image ) : 0;
	int ih = image ? EngFuncs::PIC_Height( image ) : 0;

	if( iw <= 0 || ih <= 0 )
	{
		UI_FillRect( x, y, w, h, PackRGBA( 0, 0, 0, 255 ));
		return;
	}

	float s = Q_max( (float)w / iw, (float)h / ih );
	int dw = (int)ceilf( iw * s );
	int dh = (int)ceilf( ih * s );

	EngFuncs::PIC_Set( image, 255, 255, 255, 255 );
	EngFuncs::PIC_Draw( x + ( w - dw ) / 2, y + ( h - dh ) / 2, dw, dh );
}

static void CZM_DrawFade( HIMAGE image, int x, int y, int w, int h, int alpha )
{
	if( !image )
		return;

	EngFuncs::PIC_Set( image, 255, 255, 255, alpha );
	EngFuncs::PIC_DrawTrans( x, y, w, h );
}

static void CZM_DrawTitle( int x, int y, int h, int alpha = 255 )
{
	int iw, ih;

	if( !s_hTitle )
		return;

	iw = EngFuncs::PIC_Width( s_hTitle );
	ih = EngFuncs::PIC_Height( s_hTitle );
	if( iw <= 0 || ih <= 0 )
		return;

	EngFuncs::PIC_Set( s_hTitle, 255, 255, 255, alpha );
	EngFuncs::PIC_DrawTrans( x, y, h * iw / ih, h );
}

static int CZM_FontHeight( HFont font )
{
	return g_FontMgr->GetFontTall( font );
}

static int CZM_TextWidth( HFont font, const char *text )
{
	return g_FontMgr->GetTextWideScaled( font, text, CZM_FontHeight( font ));
}

// single line, cut with an ellipsis when it does not fit into w
static void CZM_DrawText( HFont font, int x, int y, int w, const char *text, unsigned int color, uint justify = QM_LEFT )
{
	int h = CZM_FontHeight( font );

	if( !text || !text[0] )
		return;

	UI_DrawString( font, x, y, w, h, text, color, h, justify | QM_TOP, ETF_NO_WRAP | ETF_FORCECOL );
}

static void CZM_FormatMegabytes( char *buf, size_t size, int bytes )
{
	snprintf( buf, size, "%.1f", bytes / ( 1024.0f * 1024.0f ));
	for( char *p = buf; *p; p++ )
	{
		if( *p == '.' )
			*p = ',';
	}
}

/*
=================
connection errors
=================
*/
static void CZM_CleanReason( const char *in, char *out, size_t size )
{
	size_t len = 0;
	const char *s = in;

	// skip the engine's "Server message" header line
	if( !strncmp( s, "^3", 2 ) && strchr( s, '\n' ))
		s = strchr( s, '\n' ) + 1;

	while( *s && len + 1 < size )
	{
		if( s[0] == '^' && s[1] >= '0' && s[1] <= '9' )
		{
			s += 2;
			continue;
		}

		out[len++] = *s++;
	}

	while( len > 0 && ( out[len - 1] == '\n' || out[len - 1] == ' ' ))
		len--;

	out[len] = 0;
}

// ASCII case-insensitive search, server messages are in English
static bool CZM_Contains( const char *text, const char *word )
{
	for( const char *p = text; *p; p++ )
	{
		const char *a = p, *b = word;

		while( *a && *b && tolower( (unsigned char)*a ) == tolower( (unsigned char)*b ))
			a++, b++;

		if( !*b )
			return true;
	}

	return false;
}

static void CZM_ShowPendingError( void )
{
	const char *error = EngFuncs::GetCvarString( "czm_loading_error" );
	char reason[256], text[512];

	if( !error || !error[0] || !uiStatic.menu.IsActive( ))
		return;

	CZM_CleanReason( error, reason, sizeof( reason ));

	if( !strcmp( reason, "#connect_failed" ))
		snprintf( text, sizeof( text ), "Не удалось подключиться к серверу.\nПроверь интернет или попробуй позже." );
	else if( CZM_Contains( reason, "full" ))
		snprintf( text, sizeof( text ), "Сервер заполнен.\nПопробуй позже или выбери другой сервер." );
	else if( CZM_Contains( reason, "password" ))
		snprintf( text, sizeof( text ), "Неверный пароль сервера." );
	else if( CZM_Contains( reason, "ban" ))
		snprintf( text, sizeof( text ), "Доступ на этот сервер закрыт.\n%s", reason );
	else if( CZM_Contains( reason, "kick" ))
		snprintf( text, sizeof( text ), "Сервер отключил тебя.\n%s", reason );
	else
		snprintf( text, sizeof( text ), "Не удалось подключиться к серверу.\n%s", reason );

	EngFuncs::CvarSetString( "czm_loading_error", "" );
	UI_ShowMessageBox( text );
}

/*
=================
main menu background
=================
*/
bool CZM_DrawMenuBackground( Point pos, Size size )
{
	CMenuBaseWindow *current;

	CZM_LoadImages();

	if( !s_hMenuArt )
		return false;

	CZM_DrawCover( s_hMenuArt, pos.x, pos.y, size.w, size.h );
	CZM_DrawFade( s_hFadeLeft, pos.x, pos.y, size.w * 0.58f, size.h, 225 );
	CZM_DrawFade( s_hFadeBottom, pos.x, pos.y + size.h * 0.62f, size.w, size.h * 0.38f + 1, 140 );

	// the title belongs to the main menu only, other windows have their own headers
	current = uiStatic.menu.Current();
	if( current && current->szName && !strcmp( current->szName, "CMenuMain" ))
		CZM_DrawTitle( pos.x + size.w * 0.06f, pos.y + size.h * 0.075f, size.h * 0.16f );

	return true;
}

/*
=================
loading screen
=================
*/
static void CZM_DrawInfoRow( int x, int y, int w )
{
	const char *map = EngFuncs::GetCvarString( "czm_loading_map" );
	const char *players = EngFuncs::GetCvarString( "czm_loading_players" );
	const char *ping = EngFuncs::GetCvarString( "czm_loading_ping" );
	const unsigned int label = PackRGBA( 160, 168, 178, 255 );
	const unsigned int value = PackRGBA( 240, 240, 240, 255 );
	const char *separator = "   ·   ";
	char pingText[32];
	struct { const char *label, *value; } parts[3];
	int count = 0, right = x + w;

	if( map && map[0] )
		parts[count].label = "Карта  ", parts[count++].value = map;
	if( players && players[0] )
		parts[count].label = "Игроки  ", parts[count++].value = players;
	if( ping && ping[0] )
	{
		snprintf( pingText, sizeof( pingText ), "%s мс", ping );
		parts[count].label = "Пинг  ", parts[count++].value = pingText;
	}

	for( int i = 0; i < count && x < right; i++ )
	{
		if( i > 0 )
		{
			CZM_DrawText( s_hFontInfo, x, y, right - x, separator, label );
			x += CZM_TextWidth( s_hFontInfo, separator );
		}

		CZM_DrawText( s_hFontInfo, x, y, right - x, parts[i].label, label );
		x += CZM_TextWidth( s_hFontInfo, parts[i].label );
		CZM_DrawText( s_hFontInfo, x, y, right - x, parts[i].value, value );
		x += CZM_TextWidth( s_hFontInfo, parts[i].value );
	}
}

static void CZM_StageText( int stage, char *buf, size_t size )
{
	switch( stage )
	{
	case CZM_STAGE_LOCAL:
		snprintf( buf, size, "Запуск игры…" );
		break;
	case CZM_STAGE_HANDSHAKE:
		snprintf( buf, size, "Получение данных сервера…" );
		break;
	case CZM_STAGE_DOWNLOAD:
	{
		int files_done = 0, files_total = 0, bytes_done = 0, bytes_total = 0, speed = 0;
		char done[32], total[32], rate[32];

		sscanf( EngFuncs::GetCvarString( "czm_loading_dl" ), "%d %d %d %d %d",
			&files_done, &files_total, &bytes_done, &bytes_total, &speed );

		if( files_total <= 0 )
		{
			snprintf( buf, size, "Скачивание файлов сервера…" );
			break;
		}

		files_done = Q_min( files_done + 1, files_total ); // the file being downloaded now

		if( speed >= 1024 * 1024 )
		{
			char mb[32];
			CZM_FormatMegabytes( mb, sizeof( mb ), speed );
			snprintf( rate, sizeof( rate ), "%s МБ/с", mb );
		}
		else if( speed > 0 )
			snprintf( rate, sizeof( rate ), "%d КБ/с", Q_max( 1, speed / 1024 ));
		else rate[0] = 0;

		if( bytes_total > 0 )
		{
			CZM_FormatMegabytes( done, sizeof( done ), bytes_done );
			CZM_FormatMegabytes( total, sizeof( total ), bytes_total );
			snprintf( buf, size, "Скачивание файлов сервера   ·   %d из %d   ·   %s из %s МБ%s%s",
				files_done, files_total, done, total, rate[0] ? "   ·   " : "", rate );
		}
		else
		{
			snprintf( buf, size, "Скачивание файлов сервера   ·   %d из %d%s%s",
				files_done, files_total, rate[0] ? "   ·   " : "", rate );
		}
		break;
	}
	case CZM_STAGE_PRECACHE:
		snprintf( buf, size, "Загрузка карты и моделей…" );
		break;
	case CZM_STAGE_ENTER:
		snprintf( buf, size, "Вход в игру…" );
		break;
	case CZM_STAGE_CONNECT:
	default:
		snprintf( buf, size, "Подключение к серверу…" );
		break;
	}
}

static void CZM_DrawProgressBar( int x0, int x1, int y, int h, float percent )
{
	const int count = 40;
	int gap = Q_max( 2, (int)( ScreenWidth * 0.0032f ));
	float segment = (float)( x1 - x0 - gap * ( count - 1 )) / count;
	float filled = bound( 0.0f, percent, 100.0f ) * count / 100.0f;

	for( int i = 0; i < count; i++ )
	{
		int sx = x0 + (int)( i * ( segment + gap ));
		int sw = (int)segment;
		float fraction = bound( 0.0f, filled - i, 1.0f );

		UI_FillRect( sx, y, sw, h, PackRGBA( 255, 255, 255, 46 ));

		if( fraction > 0.0f )
		{
			float k = (float)i / ( count - 1 );
			unsigned int color = PackRGBA( (int)( 196 + 59 * k ), (int)( 18 + 88 * k ), (int)( 18 + 8 * k ), 255 );
			UI_FillRect( sx, y, Q_max( 1, (int)( sw * fraction )), h, color );
		}
	}
}

static void CZM_DrawCancelButton( void )
{
	int x0 = ScreenWidth * CZM_CANCEL_X0, x1 = ScreenWidth * CZM_CANCEL_X1;
	int y0 = ScreenHeight * CZM_CANCEL_Y0, y1 = ScreenHeight * CZM_CANCEL_Y1;
	int border = Q_max( 2, ScreenHeight / 400 );
	int th = CZM_FontHeight( s_hFontButton );

	UI_FillRect( x0, y0, x1 - x0, y1 - y0, PackRGBA( 0, 0, 0, 110 ));
	UI_DrawRectangleExt( x0, y0, x1 - x0, y1 - y0, PackRGBA( 255, 255, 255, 170 ), border );
	CZM_DrawText( s_hFontButton, x0, y0 + ( y1 - y0 - th ) / 2, x1 - x0, "Отмена", PackRGBA( 255, 255, 255, 255 ), QM_CENTER );
}

bool CZM_DrawLoadingScreen( void )
{
	static int lastSession = -1, image = -1, tip = 0;
	const int W = ScreenWidth, H = ScreenHeight;
	int session, stage, x0, x1, percentTop;
	float percent;
	char text[256];

	if( EngFuncs::GetCvarFloat( "czm_loading" ) == 0.0f )
	{
		CZM_ShowPendingError();
		return false;
	}

	CZM_LoadImages();

	// new picture and tip for every loading
	session = (int)EngFuncs::GetCvarFloat( "czm_loading_session" );
	if( session != lastSession )
	{
		static bool seeded = false;
		int next;

		if( !seeded )
		{
			srand( (unsigned int)time( NULL ));
			seeded = true;
		}

		do next = rand() % CZM_LOADING_IMAGES;
		while( next == image && CZM_LOADING_IMAGES > 1 );

		image = next;
		tip = rand() % (int)( sizeof( s_szTips ) / sizeof( s_szTips[0] ));
		lastSession = session;
	}

	stage = (int)EngFuncs::GetCvarFloat( "czm_loading_stage" );
	percent = EngFuncs::GetCvarFloat( "scr_loading" );

	CZM_DrawCover( s_hLoading[image], 0, 0, W, H );
	CZM_DrawFade( s_hFadeLeft, 0, 0, W * 0.62f, H, 150 );
	CZM_DrawFade( s_hFadeBottom, 0, H * 0.36f, W, H * 0.64f + 1, 240 );
	CZM_DrawTitle( W * 0.045f, H * 0.06f, H * 0.07f );

	x0 = W * 0.045f;
	x1 = W * 0.775f;

	CZM_DrawText( s_hFontHost, x0, H * 0.56f, x1 - x0, EngFuncs::GetCvarString( "czm_loading_host" ), PackRGBA( 255, 255, 255, 255 ));
	CZM_DrawInfoRow( x0, H * 0.655f, x1 - x0 );
	CZM_DrawText( s_hFontTip, x0, H * 0.712f, x1 - x0, s_szTips[tip], PackRGBA( 205, 205, 205, 255 ));

	CZM_StageText( stage, text, sizeof( text ));
	percentTop = H * 0.845f - CZM_FontHeight( s_hFontPercent );
	CZM_DrawText( s_hFontStatus, x0, H * 0.795f, ( x1 - x0 ) * 0.8f, text, PackRGBA( 235, 235, 235, 255 ));

	snprintf( text, sizeof( text ), "%d%%", (int)percent );
	CZM_DrawText( s_hFontPercent, x0, percentTop, x1 - x0, text, PackRGBA( 255, 255, 255, 255 ), QM_RIGHT );

	CZM_DrawProgressBar( x0, x1, H * 0.852f, Q_max( 4, (int)( H * 0.024f )), percent );
	CZM_DrawCancelButton();

	return true;
}

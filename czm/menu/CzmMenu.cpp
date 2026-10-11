/*
CzmMenu.cpp - Counter Zombie Mobile menu: shared drawing, language, servers, loading screen
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
#define CZM_TIPS 10

/*
=================
language
=================
*/
static const struct { const char *key, *ru, *en; } s_strings[] =
{
	// loading screen
	{ "tip1", "Совет: зомби быстрее людей — держись ближе к своим и прикрывай спину.", "Tip: zombies are faster than humans — stay close to your team." },
	{ "tip2", "Совет: в узком проходе толпу зомби сдерживать намного проще.", "Tip: a narrow corridor makes a zombie horde much easier to hold." },
	{ "tip3", "Совет: перезаряжайся заранее, пока зомби далеко.", "Tip: reload early, while the zombies are still far away." },
	{ "tip4", "Совет: не стой на открытом месте — ищи укрытие и высоту.", "Tip: don't stand in the open — find cover and high ground." },
	{ "tip5", "Совет: дробовик и пулемёт хорошо останавливают зомби вблизи.", "Tip: shotguns and machine guns stop zombies up close." },
	{ "tip6", "Совет: граната помогает отбросить толпу и выиграть пару секунд.", "Tip: a grenade pushes the horde back and buys you a few seconds." },
	{ "tip7", "Совет: целься в голову — так зомби падают быстрее.", "Tip: aim for the head — zombies go down faster." },
	{ "tip8", "Совет: стал зомби — нападай вместе с другими, по одному не ходи.", "Tip: turned into a zombie? Attack together, never alone." },
	{ "tip9", "Совет: с друзьями выживать проще — зови их в игру.", "Tip: surviving is easier with friends — invite them to play." },
	{ "tip10", "Совет: настрой управление под себя в разделе «Управление».", "Tip: tune your controls in the Controls section." },
	{ "st_local", "Запуск игры…", "Starting the game…" },
	{ "st_connect", "Подключение к серверу…", "Connecting to the server…" },
	{ "st_handshake", "Получение данных сервера…", "Receiving server data…" },
	{ "st_download", "Скачивание файлов сервера", "Downloading server files" },
	{ "st_precache", "Загрузка карты и моделей…", "Loading the map and models…" },
	{ "st_enter", "Вход в игру…", "Entering the game…" },
	{ "of", "из", "of" },
	{ "mb", "МБ", "MB" },
	{ "mbps", "МБ/с", "MB/s" },
	{ "kbps", "КБ/с", "KB/s" },
	{ "lbl_map", "Карта", "Map" },
	{ "lbl_players", "Игроки", "Players" },
	{ "lbl_ping", "Пинг", "Ping" },
	{ "ms", "мс", "ms" },
	{ "cancel", "Отмена", "Cancel" },
	// connection errors
	{ "err_connect", "Не удалось подключиться к серверу.\nПроверь интернет или попробуй позже.", "Could not connect to the server.\nCheck your connection or try again later." },
	{ "err_full", "Сервер заполнен.\nПопробуй позже или выбери другой режим.", "The server is full.\nTry again later or pick another mode." },
	{ "err_password", "Неверный пароль сервера.", "Wrong server password." },
	{ "err_banned", "Доступ на этот сервер закрыт.", "You are not allowed on this server." },
	{ "err_kicked", "Сервер отключил тебя.", "The server disconnected you." },
	{ "err_generic", "Не удалось подключиться к серверу.", "Could not connect to the server." },
	// main screen
	{ "nick_hint", "Нажми, чтобы сменить ник", "Tap to change your nickname" },
	{ "in_game", "в игре", "playing" },
	{ "querying", "Опрашиваем сервер…", "Checking the server…" },
	{ "offline", "Сервер не отвечает", "Server is not responding" },
	{ "full", "Сервер полон", "Server is full" },
	{ "nav_profile", "Профиль", "Profile" },
	{ "nav_controls", "Управление", "Controls" },
	{ "nav_settings", "Настройки", "Settings" },
	{ "nav_about", "Об игре", "About" },
	{ "bell_title", "Уведомления", "Notifications" },
	{ "bell_empty", "Пока нет уведомлений", "No notifications yet" },
	{ "about_title", "Об игре", "About" },
	{ "about_version", "Версия", "Version" },
	{ "about_engine", "Движок Xash3D FWGS и клиент CS16Client — свободное ПО (GPL).", "Xash3D FWGS engine and CS16Client — free software (GPL)." },
	{ "about_source", "Исходный код: github.com/kurostreet/cs16-client", "Source code: github.com/kurostreet/cs16-client" },
	{ "about_fonts", "Шрифты Inter — SIL Open Font License 1.1.", "Inter fonts — SIL Open Font License 1.1." },
	{ "about_icons", "Иконки: game-icons.net (Lorc, Delapouite, Skoll, sbed), CC BY 3.0.", "Icons: game-icons.net (Lorc, Delapouite, Skoll, sbed), CC BY 3.0." },
	{ "quit_title", "Выйти из игры?", "Quit the game?" },
	{ "quit_yes", "Выйти", "Quit" },
	{ "pause", "ПАУЗА", "PAUSED" },
	{ "resume", "Продолжить", "Resume" },
	{ "settings", "Настройки", "Settings" },
	{ "to_menu", "Выйти в меню", "Leave to menu" },
	// settings
	{ "set_title", "НАСТРОЙКИ", "SETTINGS" },
	{ "tab_controls", "Управление", "Controls" },
	{ "tab_graphics", "Графика", "Graphics" },
	{ "tab_sound", "Звук", "Sound" },
	{ "tab_game", "Игра", "Game" },
	{ "tab_profile", "Профиль", "Profile" },
	{ "s_look", "Обзор", "Look" },
	{ "s_look_hint", "Как быстро поворачивается камера", "How fast the camera turns" },
	{ "s_zoom", "В прицеле", "While aiming" },
	{ "s_zoom_hint", "Чувствительность с оптикой", "Sensitivity with a scope" },
	{ "s_move", "Ходьба", "Movement" },
	{ "s_move_hint", "Чем больше, тем короче движение пальца", "Higher means a shorter finger move" },
	{ "s_gyro", "Гироскоп", "Gyroscope" },
	{ "s_gyro_hint", "Наклон телефона двигает прицел", "Tilt the phone to aim" },
	{ "s_gyro_none", "На этом телефоне нет гироскопа", "This phone has no gyroscope" },
	{ "s_gyro_power", "Сила гироскопа", "Gyroscope strength" },
	{ "s_scheme", "Схема кнопок", "Button layout" },
	{ "s_scheme_hint", "Три раскладки, у каждой свои изменения", "Three layouts, each keeps its own changes" },
	{ "opt_scheme1", "Классика", "Classic" },
	{ "opt_scheme2", "Шутер", "Shooter" },
	{ "opt_scheme3", "Минимум", "Minimal" },
	{ "s_buttons", "Кнопки на экране", "On-screen buttons" },
	{ "s_buttons_hint", "Место, размер, иконки, новые кнопки", "Place, size, icons, new buttons" },
	{ "b_edit", "Изменить", "Edit" },
	{ "b_reset", "Сбросить", "Reset" },
	{ "reset_confirm", "Сбросить раскладку кнопок?", "Reset the button layout?" },
	{ "reset_done", "Раскладка кнопок сброшена", "Button layout reset" },
	{ "s_brightness", "Яркость", "Brightness" },
	{ "s_fps", "Лимит FPS", "FPS limit" },
	{ "s_fps_hint", "Больше — плавнее, но телефон греется", "Higher is smoother but warms the phone" },
	{ "s_showfps", "FPS на экране", "Show FPS" },
	{ "s_showping", "Пинг на экране", "Show ping" },
	{ "s_effects", "Эффекты", "Effects" },
	{ "s_effects_hint", "Гильзы, дым и тела. Меньше — быстрее", "Shells, smoke and bodies. Less is faster" },
	{ "opt_low", "Мало", "Low" },
	{ "opt_mid", "Средне", "Medium" },
	{ "opt_high", "Много", "High" },
	{ "s_volume", "Громкость", "Volume" },
	{ "s_music", "Музыка в меню", "Menu music" },
	{ "s_voice", "Голосовой чат", "Voice chat" },
	{ "s_voice_hint", "Слышать игроков и говорить", "Hear and talk to players" },
	{ "s_voice_in", "Громкость голосов", "Voice volume" },
	{ "s_voice_out", "Громкость микрофона", "Microphone volume" },
	{ "s_lang", "Язык", "Language" },
	{ "s_xhair", "Прицел", "Crosshair" },
	{ "s_xhair_hint", "Так он выглядит в игре", "How it looks in the game" },
	{ "s_xhair_size", "Размер прицела", "Crosshair size" },
	{ "opt_auto", "Авто", "Auto" },
	{ "opt_small", "Малый", "Small" },
	{ "opt_medium", "Средний", "Medium" },
	{ "opt_large", "Большой", "Large" },
	{ "s_xhair_color", "Цвет прицела", "Crosshair color" },
	{ "s_xhair_trans", "Прозрачный прицел", "Translucent crosshair" },
	{ "s_hand", "Оружие в руке", "Weapon hand" },
	{ "opt_right", "Правой", "Right" },
	{ "opt_left", "Левой", "Left" },
	{ "s_autoswitch", "Брать лучшее оружие", "Switch to better weapons" },
	{ "s_autoswitch_hint", "Подобрал оружие мощнее — оно сразу в руке", "A stronger weapon you pick up is used at once" },
	{ "s_buymenu", "Меню покупки", "Buy menu" },
	{ "opt_new", "Новое", "New" },
	{ "opt_classic", "Классика", "Classic" },
	{ "s_nick", "Ник", "Nickname" },
	{ "s_nick_hint", "Так тебя видят другие игроки", "How other players see you" },
	{ "nick_placeholder", "Твой ник", "Your nickname" },
	{ "nick_bad", "Введи свой ник, от 2 символов", "Enter your nickname, 2 characters or more" },
	{ "s_spray", "Спрей", "Spray" },
	{ "s_spray_hint", "Рисуется кнопкой спрея в игре", "Drawn with the spray button in the game" },
	// first start
	{ "setup_title", "Добро пожаловать!", "Welcome!" },
	{ "setup_text", "Выбери язык и придумай ник", "Choose a language and a nickname" },
	{ "setup_go", "Играть", "Play" },
};

static bool s_bInitialized;

void CZM_Init( void )
{
	if( s_bInitialized )
		return;

	// archived cvars must exist before config.cfg runs, the menu creates its fonts early enough
	s_bInitialized = true;
	EngFuncs::CvarRegister( "czm_lang", "", FCVAR_ARCHIVE );
	EngFuncs::CvarRegister( "czm_setup", "0", FCVAR_ARCHIVE );
	EngFuncs::CvarRegister( "czm_defaults", "0", FCVAR_ARCHIVE );
	EngFuncs::CvarRegister( "czm_zoom_sens", "1.2", FCVAR_ARCHIVE );
	EngFuncs::CvarRegister( "czm_connect_name", "", 0 );
	EngFuncs::CvarRegister( "czm_connect_adr", "", 0 );
	EngFuncs::CvarRegister( "czm_touch_scheme", "1", FCVAR_ARCHIVE );
	EngFuncs::CvarRegister( "czm_touch_version", "0", FCVAR_ARCHIVE );
}

// the phone language, passed by the launcher
static bool CZM_LocaleIsRussian( void )
{
	const char *locale = getenv( "CZM_LOCALE" );

	return !locale || !locale[0] || !strncmp( locale, "ru", 2 ) || !strncmp( locale, "uk", 2 )
		|| !strncmp( locale, "be", 2 ) || !strncmp( locale, "kk", 2 );
}

bool CZM_IsRussian( void )
{
	const char *lang;

	CZM_Init();

	// the player's choice, saved in config.cfg, wins over the phone language
	lang = EngFuncs::GetCvarString( "czm_lang" );
	if( lang && lang[0] )
		return !strcmp( lang, "ru" );

	return CZM_LocaleIsRussian();
}

const char *CZM_T( const char *key )
{
	bool russian = CZM_IsRussian();

	for( size_t i = 0; i < sizeof( s_strings ) / sizeof( s_strings[0] ); i++ )
	{
		if( !strcmp( s_strings[i].key, key ))
			return russian ? s_strings[i].ru : s_strings[i].en;
	}

	return key;
}

void CZM_SetLanguage( bool russian )
{
	CZM_Init();
	EngFuncs::CvarSetString( "czm_lang", russian ? "ru" : "en" );
	// the stock menus read their translations from ui_language on the next start
	EngFuncs::CvarSetString( "ui_language", russian ? "russian" : "english" );
	EngFuncs::ClientCmd( false, "host_writeconfig\n" );
}

/*
=================
fonts
=================
*/
static HFont s_hFonts[CZMF_COUNT];

bool CZM_FindFontFile( const char *name, char *dataFile, size_t dataFileChars )
{
	static const struct { const char *name, *file; } fonts[] =
	{
		{ "CZM Black", "gfx/fonts/czm/InterDisplay-Black.otf" },
		{ "CZM BlackItalic", "gfx/fonts/czm/InterDisplay-BlackItalic.otf" },
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
	CZM_Init();

	static const struct { const char *name; int tall, weight; } specs[CZMF_COUNT] =
	{
		// sizes are in 768-line units, like the rest of the menu
		{ "CZM Black", 46, 900 },       // CZMF_LOAD_HOST
		{ "CZM Black", 40, 900 },       // CZMF_LOAD_PERCENT
		{ "CZM Semibold", 25, 600 },    // CZMF_LOAD_INFO
		{ "CZM Semibold", 23, 600 },    // CZMF_LOAD_STATUS
		{ "CZM Medium", 21, 500 },      // CZMF_LOAD_TIP
		{ "CZM Semibold", 26, 600 },    // CZMF_BUTTON
		{ "CZM BlackItalic", 42, 900 }, // CZMF_TILE_TITLE
		{ "CZM Medium", 19, 500 },      // CZMF_TILE_TEXT
		{ "CZM Semibold", 21, 600 },    // CZMF_TILE_STATUS
		{ "CZM Semibold", 29, 600 },    // CZMF_NICK
		{ "CZM Medium", 17, 500 },      // CZMF_SMALL
		{ "CZM Semibold", 20, 600 },    // CZMF_NAV
		{ "CZM Semibold", 24, 600 },    // CZMF_LANG
		{ "CZM BlackItalic", 42, 900 }, // CZMF_PANEL_TITLE
		{ "CZM Medium", 21, 500 },      // CZMF_PANEL_TEXT
	};

	for( int i = 0; i < CZMF_COUNT; i++ )
		s_hFonts[i] = CFontBuilder( specs[i].name, specs[i].tall * scale, specs[i].weight ).Create();
}

HFont CZM_Font( int id )
{
	return ( id >= 0 && id < CZMF_COUNT ) ? s_hFonts[id] : uiStatic.hDefaultFont;
}

int CZM_FontHeight( HFont font )
{
	return g_FontMgr->GetFontTall( font );
}

int CZM_TextWidth( HFont font, const char *text )
{
	if( !text || !text[0] )
		return 0;
	return g_FontMgr->GetTextWideScaled( font, text, CZM_FontHeight( font ));
}

void CZM_DrawText( HFont font, int x, int y, int w, const char *text, unsigned int color, unsigned int justify )
{
	int h = CZM_FontHeight( font );

	if( !text || !text[0] || w <= 0 )
		return;

	UI_DrawString( font, x, y, w, h, text, color, h, justify | QM_TOP, ETF_NO_WRAP | ETF_FORCECOL );
}

/*
=================
images
=================
*/
static struct { char path[64]; HIMAGE handle; } s_images[64];
static int s_numImages;

HIMAGE CZM_Image( const char *path )
{
	for( int i = 0; i < s_numImages; i++ )
	{
		if( !strcmp( s_images[i].path, path ))
			return s_images[i].handle;
	}

	HIMAGE handle = EngFuncs::PIC_Load( path );
	if( s_numImages < (int)( sizeof( s_images ) / sizeof( s_images[0] )))
	{
		Q_strncpy( s_images[s_numImages].path, path, sizeof( s_images[0].path ));
		s_images[s_numImages].handle = handle;
		s_numImages++;
	}

	return handle;
}

void CZM_DrawImage( const char *path, int x, int y, int w, int h, unsigned int color, bool transparent )
{
	HIMAGE image = CZM_Image( path );
	int r, g, b, a;

	if( !image || w <= 0 || h <= 0 )
		return;

	UnpackRGBA( r, g, b, a, color );
	EngFuncs::PIC_Set( image, r, g, b, a );
	if( transparent )
		EngFuncs::PIC_DrawTrans( x, y, w, h );
	else
		EngFuncs::PIC_Draw( x, y, w, h );
}

void CZM_DrawSliced( const char *path, int x, int y, int w, int h, int imageBorder, int screenBorder, unsigned int color )
{
	HIMAGE image = CZM_Image( path );
	int iw, ih, r, g, b, a;

	if( !image || w <= 0 || h <= 0 )
		return;

	iw = EngFuncs::PIC_Width( image );
	ih = EngFuncs::PIC_Height( image );
	imageBorder = Q_min( imageBorder, Q_min( iw, ih ) / 2 - 1 );
	screenBorder = Q_min( screenBorder, Q_min( w, h ) / 2 );
	UnpackRGBA( r, g, b, a, color );

	const int xs[4] = { x, x + screenBorder, x + w - screenBorder, x + w };
	const int ys[4] = { y, y + screenBorder, y + h - screenBorder, y + h };
	const int us[4] = { 0, imageBorder, iw - imageBorder, iw };
	const int vs[4] = { 0, imageBorder, ih - imageBorder, ih };

	for( int j = 0; j < 3; j++ )
	{
		for( int i = 0; i < 3; i++ )
		{
			wrect_t rc;

			if( xs[i + 1] <= xs[i] || ys[j + 1] <= ys[j] )
				continue;

			rc.left = us[i];
			rc.right = us[i + 1];
			rc.top = vs[j];
			rc.bottom = vs[j + 1];
			// the engine resets the colour after every picture
			EngFuncs::PIC_Set( image, r, g, b, a );
			EngFuncs::PIC_DrawTrans( xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j], &rc );
		}
	}
}

void CZM_DrawPill( const char *path, int x, int y, int w, int h, unsigned int color )
{
	HIMAGE image = CZM_Image( path );
	int iw, ih, cap, icap, r, g, b, a;

	if( !image || w <= 0 || h <= 0 )
		return;

	iw = EngFuncs::PIC_Width( image );
	ih = EngFuncs::PIC_Height( image );
	cap = Q_min( h / 2, w / 2 );
	icap = Q_min( ih / 2, iw / 2 );
	UnpackRGBA( r, g, b, a, color );

	const int xs[4] = { x, x + cap, x + w - cap, x + w };
	const int us[4] = { 0, icap, iw - icap, iw };

	for( int i = 0; i < 3; i++ )
	{
		wrect_t rc;

		if( xs[i + 1] <= xs[i] )
			continue;

		rc.left = us[i];
		rc.right = us[i + 1];
		rc.top = 0;
		rc.bottom = ih;
		EngFuncs::PIC_Set( image, r, g, b, a );
		EngFuncs::PIC_DrawTrans( xs[i], y, xs[i + 1] - xs[i], h, &rc );
	}
}

int CZM_ImageAspect1000( const char *path )
{
	HIMAGE image = CZM_Image( path );
	int w = image ? EngFuncs::PIC_Width( image ) : 0;
	int h = image ? EngFuncs::PIC_Height( image ) : 0;

	return ( w > 0 && h > 0 ) ? w * 1000 / h : 0;
}

void CZM_DrawCover( HIMAGE image, int x, int y, int w, int h )
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

unsigned int CZM_PingColor( int ping )
{
	if( ping < 80 )
		return PackRGBA( 74, 222, 128, 255 );
	if( ping < 150 )
		return PackRGBA( 250, 204, 21, 255 );
	return PackRGBA( 248, 113, 113, 255 );
}

void CZM_CleanName( const char *in, char *out, size_t size )
{
	size_t len = 0;

	for( const char *s = in ? in : ""; *s && len + 1 < size; s++ )
	{
		// colour codes like ^1
		if( s[0] == '^' && s[1] >= '0' && s[1] <= '9' )
		{
			s++;
			continue;
		}
		out[len++] = *s;
	}

	out[len] = 0;
}

bool CZM_CursorIn( int x, int y, int w, int h )
{
	return uiStatic.cursorX >= x && uiStatic.cursorX < x + w && uiStatic.cursorY >= y && uiStatic.cursorY < y + h;
}

const char *CZM_Version( void )
{
	// the app version from the launcher, the client build number otherwise
	const char *version = getenv( "CZM_APP_VERSION" );

	if( version && version[0] )
		return version;

	return EngFuncs::GetCvarString( "cscl_ver" );
}

void CZM_OpenConsole( void )
{
	EngFuncs::ClientCmd( true, "ui_allowconsole\n" );
	UI_SetActiveMenu( false );
	EngFuncs::KEY_SetDest( KEY_CONSOLE );
}

static void CZM_DrawFade( const char *path, int x, int y, int w, int h, int alpha )
{
	CZM_DrawImage( path, x, y, w, h, PackRGBA( 255, 255, 255, alpha ));
}

static void CZM_DrawTitle( int x, int y, int h )
{
	int aspect = CZM_ImageAspect1000( "gfx/czm/title.png" );

	if( aspect > 0 )
		CZM_DrawImage( "gfx/czm/title.png", x, y, h * aspect / 1000, h );
}

/*
=================
servers

Temporary list for development; later it comes from kontrafps.ru.
=================
*/
static CzmServer s_servers[] =
{
	{ "ZOMBIE ATTACK", "131.196.199.248:27015", "gfx/czm/ui/tile_attack.png", 0xFFFF3B2F },
	{ "ZOMBIE ESCAPE", "131.196.196.196:27020", "gfx/czm/ui/tile_escape.png", 0xFF22D3EE },
	{ "ZOMBIE BOSS", "104.234.65.245:27300", "gfx/czm/ui/tile_boss.png", 0xFFA855F7 },
};

int CZM_ServerCount( void )
{
	return sizeof( s_servers ) / sizeof( s_servers[0] );
}

CzmServer *CZM_Server( int index )
{
	static bool initialized = false;

	if( !initialized )
	{
		for( int i = 0; i < CZM_ServerCount(); i++ )
		{
			s_servers[i].ping = -1;
			s_servers[i].map[0] = 0;
		}
		initialized = true;
	}

	return ( index >= 0 && index < CZM_ServerCount()) ? &s_servers[index] : NULL;
}

void CZM_QueryServers( bool force )
{
	double now = EngFuncs::DoubleTime();

	for( int i = 0; i < CZM_ServerCount(); i++ )
	{
		CzmServer *server = CZM_Server( i );

		if( !force && now - server->queryTime < 6.0 )
			continue;

		// the previous query got no answer
		if( server->queryTime > 0.0 && server->answerTime < server->queryTime && now - server->queryTime > 3.0 )
			server->misses++;

		server->queryTime = now;
		EngFuncs::ClientCmdF( false, "czm_queryserver \"%s\"\n", server->address );
	}
}

bool CZM_OnServerInfo( netadr_t adr, const char *info )
{
	const char *address = EngFuncs::NET_AdrToString( adr );
	const char *value;

	for( int i = 0; i < CZM_ServerCount(); i++ )
	{
		CzmServer *server = CZM_Server( i );

		if( strcmp( server->address, address ))
			continue;

		value = Info_ValueForKey( info, "czmping" );
		if( value && value[0] )
		{
			server->ping = atoi( value );
			return true; // our own message, not a server entry
		}

		value = Info_ValueForKey( info, "map" );
		if( value )
			Q_strncpy( server->map, value, sizeof( server->map ));
		value = Info_ValueForKey( info, "numcl" );
		server->players = value ? atoi( value ) : 0;
		value = Info_ValueForKey( info, "maxcl" );
		server->maxplayers = value ? atoi( value ) : 0;
		server->answerTime = EngFuncs::DoubleTime();
		server->misses = 0;

		// rough ping until the real one arrives
		if( server->ping < 0 )
			server->ping = (int)(( server->answerTime - server->queryTime ) * 1000.0 );
		return false;
	}

	value = Info_ValueForKey( info, "czmping" );
	return value && value[0];
}

void CZM_ConnectToServer( int index )
{
	CzmServer *server = CZM_Server( index );

	if( !server )
		return;

	EngFuncs::CvarSetString( "czm_connect_name", server->title );
	EngFuncs::CvarSetString( "czm_connect_adr", server->address );
	EngFuncs::ClientCmdF( false, "connect \"%s\" gs\n", server->address );
}

static CzmServer *CZM_ServerByAddress( const char *address )
{
	for( int i = 0; address && address[0] && i < CZM_ServerCount(); i++ )
	{
		if( !strcmp( CZM_Server( i )->address, address ))
			return CZM_Server( i );
	}
	return NULL;
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
		snprintf( text, sizeof( text ), "%s", CZM_T( "err_connect" ));
	else if( CZM_Contains( reason, "full" ))
		snprintf( text, sizeof( text ), "%s", CZM_T( "err_full" ));
	else if( CZM_Contains( reason, "password" ))
		snprintf( text, sizeof( text ), "%s", CZM_T( "err_password" ));
	else if( CZM_Contains( reason, "ban" ))
		snprintf( text, sizeof( text ), "%s\n%s", CZM_T( "err_banned" ), reason );
	else if( CZM_Contains( reason, "kick" ))
		snprintf( text, sizeof( text ), "%s\n%s", CZM_T( "err_kicked" ), reason );
	else
		snprintf( text, sizeof( text ), "%s\n%s", CZM_T( "err_generic" ), reason );

	EngFuncs::CvarSetString( "czm_loading_error", "" );
	UI_ShowMessageBox( text );
}

/*
=================
menu background
=================
*/
static bool s_bGameVisible;

void CZM_SetGameVisible( bool visible )
{
	s_bGameVisible = visible;
}

bool CZM_DrawMenuBackground( Point pos, Size size )
{
	HIMAGE art = CZM_Image( "gfx/czm/menu_bg.png" );

	if( !art )
		return false;

	// in the game, keep the world visible behind the pause window and settings
	if( CL_IsActive( ))
	{
		if( !s_bGameVisible )
			UI_FillRect( pos.x, pos.y, size.w, size.h, PackRGBA( 0, 0, 0, 150 ));
		return true;
	}

	CZM_DrawCover( art, pos.x, pos.y, size.w, size.h );
	CZM_DrawFade( "gfx/czm/fade_left.png", pos.x, pos.y, size.w * 0.58f, size.h, 200 );
	CZM_DrawFade( "gfx/czm/fade_bottom.png", pos.x, pos.y + size.h * 0.62f, size.w, size.h * 0.38f + 1, 140 );
	return true;
}

/*
=================
loading screen
=================
*/
static void CZM_DrawInfoRow( int x, int y, int w, CzmServer *cached )
{
	HFont font = CZM_Font( CZMF_LOAD_INFO );
	const char *map = EngFuncs::GetCvarString( "czm_loading_map" );
	const char *players = EngFuncs::GetCvarString( "czm_loading_players" );
	const char *ping = EngFuncs::GetCvarString( "czm_loading_ping" );
	const unsigned int label = PackRGBA( 160, 168, 178, 255 );
	const unsigned int value = PackRGBA( 240, 240, 240, 255 );
	const char *separator = "   ·   ";
	char mapText[64], playersText[32], pingText[32];
	struct { const char *label, *value; } parts[3];
	int count = 0, right = x + w;

	// what the menu already knows about our server fills the gaps
	if(( !map || !map[0] ) && cached && cached->map[0] )
		map = cached->map;
	if(( !players || !players[0] ) && cached && cached->maxplayers > 0 )
	{
		snprintf( playersText, sizeof( playersText ), "%d/%d", cached->players, cached->maxplayers );
		players = playersText;
	}
	if(( !ping || !ping[0] ) && cached && cached->ping > 0 )
	{
		snprintf( pingText, sizeof( pingText ), "%d", cached->ping );
		ping = pingText;
	}

	if( map && map[0] )
	{
		Q_strncpy( mapText, map, sizeof( mapText ));
		parts[count].label = CZM_T( "lbl_map" ), parts[count++].value = mapText;
	}
	if( players && players[0] )
		parts[count].label = CZM_T( "lbl_players" ), parts[count++].value = players;
	if( ping && ping[0] )
	{
		static char pingValue[32];
		snprintf( pingValue, sizeof( pingValue ), "%s %s", ping, CZM_T( "ms" ));
		parts[count].label = CZM_T( "lbl_ping" ), parts[count++].value = pingValue;
	}

	for( int i = 0; i < count && x < right; i++ )
	{
		char label_text[64];

		if( i > 0 )
		{
			CZM_DrawText( font, x, y, right - x, separator, label );
			x += CZM_TextWidth( font, separator );
		}

		snprintf( label_text, sizeof( label_text ), "%s  ", parts[i].label );
		CZM_DrawText( font, x, y, right - x, label_text, label );
		x += CZM_TextWidth( font, label_text );
		CZM_DrawText( font, x, y, right - x, parts[i].value, value );
		x += CZM_TextWidth( font, parts[i].value );
	}
}

static void CZM_FormatMegabytes( char *buf, size_t size, int bytes )
{
	snprintf( buf, size, "%.1f", bytes / ( 1024.0f * 1024.0f ));
	if( CZM_IsRussian( ))
	{
		for( char *p = buf; *p; p++ )
		{
			if( *p == '.' )
				*p = ',';
		}
	}
}

static void CZM_StageText( int stage, char *buf, size_t size )
{
	switch( stage )
	{
	case CZM_STAGE_LOCAL:
		snprintf( buf, size, "%s", CZM_T( "st_local" ));
		break;
	case CZM_STAGE_HANDSHAKE:
		snprintf( buf, size, "%s", CZM_T( "st_handshake" ));
		break;
	case CZM_STAGE_DOWNLOAD:
	{
		int files_done = 0, files_total = 0, bytes_done = 0, bytes_total = 0, speed = 0;
		char done[32], total[32], rate[48];
		const char *sep = "   ·   ";

		sscanf( EngFuncs::GetCvarString( "czm_loading_dl" ), "%d %d %d %d %d",
			&files_done, &files_total, &bytes_done, &bytes_total, &speed );

		if( files_total <= 0 )
		{
			snprintf( buf, size, "%s…", CZM_T( "st_download" ));
			break;
		}

		files_done = Q_min( files_done + 1, files_total ); // the file being downloaded now

		if( speed >= 1024 * 1024 )
		{
			char mb[32];
			CZM_FormatMegabytes( mb, sizeof( mb ), speed );
			snprintf( rate, sizeof( rate ), "%s%s %s", sep, mb, CZM_T( "mbps" ));
		}
		else if( speed > 0 )
			snprintf( rate, sizeof( rate ), "%s%d %s", sep, Q_max( 1, speed / 1024 ), CZM_T( "kbps" ));
		else rate[0] = 0;

		if( bytes_total > 0 )
		{
			CZM_FormatMegabytes( done, sizeof( done ), bytes_done );
			CZM_FormatMegabytes( total, sizeof( total ), bytes_total );
			snprintf( buf, size, "%s%s%d %s %d%s%s %s %s %s%s", CZM_T( "st_download" ), sep, files_done, CZM_T( "of" ),
				files_total, sep, done, CZM_T( "of" ), total, CZM_T( "mb" ), rate );
		}
		else
		{
			snprintf( buf, size, "%s%s%d %s %d%s", CZM_T( "st_download" ), sep, files_done, CZM_T( "of" ), files_total, rate );
		}
		break;
	}
	case CZM_STAGE_PRECACHE:
		snprintf( buf, size, "%s", CZM_T( "st_precache" ));
		break;
	case CZM_STAGE_ENTER:
		snprintf( buf, size, "%s", CZM_T( "st_enter" ));
		break;
	case CZM_STAGE_CONNECT:
	default:
		snprintf( buf, size, "%s", CZM_T( "st_connect" ));
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
	HFont font = CZM_Font( CZMF_BUTTON );
	int x0 = ScreenWidth * CZM_CANCEL_X0, x1 = ScreenWidth * CZM_CANCEL_X1;
	int y0 = ScreenHeight * CZM_CANCEL_Y0, y1 = ScreenHeight * CZM_CANCEL_Y1;
	int border = Q_max( 2, ScreenHeight / 400 );
	int th = CZM_FontHeight( font );

	UI_FillRect( x0, y0, x1 - x0, y1 - y0, PackRGBA( 0, 0, 0, 110 ));
	UI_DrawRectangleExt( x0, y0, x1 - x0, y1 - y0, PackRGBA( 255, 255, 255, 170 ), border );
	CZM_DrawText( font, x0, y0 + ( y1 - y0 - th ) / 2, x1 - x0, CZM_T( "cancel" ), PackRGBA( 255, 255, 255, 255 ), QM_CENTER );
}

bool CZM_DrawLoadingScreen( void )
{
	static int lastSession = -1, image = 0, tip = 0;
	const int W = ScreenWidth, H = ScreenHeight;
	int session, stage, x0, x1, percentTop;
	float percent;
	char text[256];
	const char *title;
	CzmServer *cached;

	CZM_Init();

	if( EngFuncs::GetCvarFloat( "czm_loading" ) == 0.0f )
	{
		CZM_ShowPendingError();
		return false;
	}

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
		while( next == image && lastSession >= 0 && CZM_LOADING_IMAGES > 1 );

		image = next;
		tip = rand() % CZM_TIPS;
		lastSession = session;
	}

	stage = (int)EngFuncs::GetCvarFloat( "czm_loading_stage" );
	percent = EngFuncs::GetCvarFloat( "scr_loading" );
	cached = CZM_ServerByAddress( EngFuncs::GetCvarString( "czm_connect_adr" ));

	snprintf( text, sizeof( text ), "gfx/czm/loading_%d.png", image + 1 );
	CZM_DrawCover( CZM_Image( text ), 0, 0, W, H );
	CZM_DrawFade( "gfx/czm/fade_left.png", 0, 0, W * 0.62f, H, 150 );
	CZM_DrawFade( "gfx/czm/fade_bottom.png", 0, H * 0.36f, W, H * 0.64f + 1, 240 );
	CZM_DrawTitle( W * 0.045f, H * 0.06f, H * 0.07f );

	x0 = W * 0.045f;
	x1 = W * 0.775f;

	// our mode name, the server's own name only if we connect somewhere else
	title = EngFuncs::GetCvarString( "czm_connect_name" );
	if( !cached || !title || !title[0] )
		title = EngFuncs::GetCvarString( "czm_loading_host" );

	CZM_DrawText( CZM_Font( CZMF_LOAD_HOST ), x0, H * 0.56f, x1 - x0, title, PackRGBA( 255, 255, 255, 255 ));
	CZM_DrawInfoRow( x0, H * 0.655f, x1 - x0, cached );
	snprintf( text, sizeof( text ), "tip%d", tip + 1 );
	CZM_DrawText( CZM_Font( CZMF_LOAD_TIP ), x0, H * 0.712f, x1 - x0, CZM_T( text ), PackRGBA( 205, 205, 205, 255 ));

	CZM_StageText( stage, text, sizeof( text ));
	percentTop = H * 0.845f - CZM_FontHeight( CZM_Font( CZMF_LOAD_PERCENT ));
	CZM_DrawText( CZM_Font( CZMF_LOAD_STATUS ), x0, H * 0.795f, ( x1 - x0 ) * 0.8f, text, PackRGBA( 235, 235, 235, 255 ));

	snprintf( text, sizeof( text ), "%d%%", (int)percent );
	CZM_DrawText( CZM_Font( CZMF_LOAD_PERCENT ), x0, percentTop, x1 - x0, text, PackRGBA( 255, 255, 255, 255 ), QM_RIGHT );

	CZM_DrawProgressBar( x0, x1, H * 0.852f, Q_max( 4, (int)( H * 0.024f )), percent );
	CZM_DrawCancelButton();

	return true;
}

/*
=================
shared pieces of our windows
=================
*/
bool CzmRect::HasCursor( void ) const
{
	return Contains( uiStatic.cursorX, uiStatic.cursorY );
}

CzmRect CZM_Rect( int x, int y, int w, int h )
{
	CzmRect r = { x, y, w, h };
	return r;
}

unsigned int CZM_White( int alpha )
{
	return PackRGBA( 255, 255, 255, alpha );
}

unsigned int CZM_Grey( void )
{
	return PackRGBA( 170, 176, 186, 255 );
}

int CZM_LineWidth( void )
{
	return Q_max( 1, ScreenHeight / 600 );
}

void CZM_PlayClick( void )
{
	EngFuncs::PlayLocalSound( uiStatic.sounds[SND_LAUNCH] );
}

void CZM_DrawPanel( const CzmRect &r )
{
	CZM_DrawSliced( "gfx/czm/ui/panel.png", r.x, r.y, r.w, r.h, 40, (int)( ScreenHeight * 0.026f ));
}

void CZM_DrawButton( const char *image, const CzmRect &r, const char *label, unsigned int textColor, bool pressed, int font )
{
	HFont hfont = CZM_Font( font );

	CZM_DrawPill( image, r.x, r.y, r.w, r.h, pressed ? PackRGBA( 185, 185, 185, 255 ) : CZM_White());
	CZM_DrawText( hfont, r.x + r.h / 4, r.y + ( r.h - CZM_FontHeight( hfont )) / 2, r.w - r.h / 2, label, textColor, QM_CENTER );
}

/*
=================
text input
=================
*/
void CzmTextEdit::Begin( const char *initial, const CzmRect &r )
{
	Q_strncpy( text, initial ? initial : "", sizeof( text ));
	active = true;
	EngFuncs::SetTextInputRect( r.x, r.y, r.w, r.h );
	UI_EnableTextInput( true );
}

void CzmTextEdit::End( void )
{
	if( !active )
		return;

	active = false;
	UI_EnableTextInput( false );
}

bool CzmTextEdit::KeyDown( int key )
{
	if( !active )
		return false;

	if( UI::Key::IsBackspace( key ))
	{
		size_t len = strlen( text );

		// remove one whole UTF-8 character
		if( len > 0 )
		{
			len--;
			while( len > 0 && ( (unsigned char)text[len] & 0xC0 ) == 0x80 )
				len--;
			text[len] = 0;
		}
		return true;
	}

	if( UI::Key::IsEnter( key ))
	{
		End();
		return true;
	}

	return false;
}

void CzmTextEdit::Char( int ch )
{
	size_t len;

	// characters that break names in commands and colour codes
	if( !active || ch < 32 || ch == '"' || ch == '\\' || ch == '^' || ch == ';' || ch == '%' )
		return;

	len = strlen( text );

	// keep room for a whole UTF-8 character
	if( len + ( ch >= 0xC0 ? 4 : 1 ) >= sizeof( text ))
		return;

	text[len] = (char)ch;
	text[len + 1] = 0;
}

void CZM_DrawField( const CzmRect &r, const CzmTextEdit &edit, const char *value, const char *placeholder, bool pressed )
{
	HFont font = CZM_Font( CZMF_TILE_STATUS );
	int pad = r.h * 0.45f;
	int textY = r.y + ( r.h - CZM_FontHeight( font )) / 2;
	int pencil = r.h * 0.40f;
	const char *text = edit.active ? edit.text : value;

	CZM_DrawPill( edit.active ? "gfx/czm/ui/button_danger.png" : "gfx/czm/ui/button_secondary.png",
		r.x, r.y, r.w, r.h, pressed ? PackRGBA( 200, 200, 200, 255 ) : CZM_White());

	if( text && text[0] )
		CZM_DrawText( font, r.x + pad, textY, r.w - pad * 2 - pencil, text, CZM_White());
	else
		CZM_DrawText( font, r.x + pad, textY, r.w - pad * 2 - pencil, placeholder, PackRGBA( 130, 136, 146, 255 ));

	if( edit.active )
	{
		// blinking cursor after the text
		if(( uiStatic.realTime / 500 ) % 2 == 0 )
		{
			int x = r.x + pad + Q_min( CZM_TextWidth( font, text ), r.w - pad * 2 - pencil ) + 2;
			UI_FillRect( x, textY, Q_max( 2, ScreenHeight / 400 ), CZM_FontHeight( font ), PackRGBA( 239, 68, 68, 255 ));
		}
	}
	else
	{
		CZM_DrawImage( "gfx/czm/ui/icon_pencil.png", r.x + r.w - pad - pencil, r.y + ( r.h - pencil ) / 2, pencil, pencil,
			PackRGBA( 200, 205, 215, 255 ));
	}
}

/*
=================
nickname and sprays
=================
*/
static int CZM_Utf8Length( const char *s )
{
	int count = 0;

	for( ; *s; s++ )
	{
		if(( (unsigned char)*s & 0xC0 ) != 0x80 )
			count++;
	}

	return count;
}

bool CZM_NickIsValid( const char *nick )
{
	// names the engine and stock configs give everyone, servers dislike them
	static const char *stock[] = { "CS_Mobile_Xash3D", "Player", "unnamed" };
	char clean[64];
	const char *s;
	int letters = 0;

	if( !nick )
		return false;

	CZM_CleanName( nick, clean, sizeof( clean ));

	for( s = clean; *s; s++ )
	{
		if( *s != ' ' )
			letters++;
	}

	if( letters < 2 || CZM_Utf8Length( clean ) < 2 )
		return false;

	for( size_t i = 0; i < sizeof( stock ) / sizeof( stock[0] ); i++ )
	{
		if( !stricmp( clean, stock[i] ))
			return false;
	}

	return true;
}

void CZM_SetNick( const char *nick )
{
	EngFuncs::CvarSetString( "name", nick );
}

static const char *s_sprays[] =
{
	"czm_logo", "czm_zombie", "czm_hunter", "czm_soldier", "czm_biohazard",
	"czm_skull", "czm_claws", "czm_gg", "czm_zm",
};

int CZM_SprayCount( void )
{
	return sizeof( s_sprays ) / sizeof( s_sprays[0] );
}

const char *CZM_SprayName( int index )
{
	return ( index >= 0 && index < CZM_SprayCount()) ? s_sprays[index] : "";
}

const char *CZM_SprayImage( int index )
{
	static char path[64];

	snprintf( path, sizeof( path ), "logos/%s.png", CZM_SprayName( index ));
	return path;
}

int CZM_CurrentSpray( void )
{
	const char *current = EngFuncs::GetCvarString( "cl_logofile" );

	for( int i = 0; current && i < CZM_SprayCount(); i++ )
	{
		if( !stricmp( current, s_sprays[i] ))
			return i;
	}

	return -1;
}

void CZM_SetSpray( int index )
{
	int length = 0;
	byte *data;

	if( index < 0 || index >= CZM_SprayCount())
		return;

	// the engine packs logos/remapped.png into tempdecal.wad for the server
	data = EngFuncs::COM_LoadFile( CZM_SprayImage( index ), &length );
	if( !data || length <= 0 )
		return;

	EngFuncs::DeleteFile( "logos/remapped.bmp" );
	EngFuncs::COM_SaveFile( "logos/remapped.png", data, length );
	EngFuncs::COM_FreeFile( data );

	EngFuncs::CvarSetString( "cl_logoext", "png" );
	EngFuncs::CvarSetString( "cl_logofile", s_sprays[index] );
	EngFuncs::CvarSetValue( "@cl_logoupdate", !EngFuncs::GetCvarFloat( "@cl_logoupdate" ));
}

/*
=================
first start and startup settings
=================
*/
static void CZM_RegisterSettingsCvars( void )
{
	CZM_Init();
}

bool CZM_FirstStart( void )
{
	CZM_RegisterSettingsCvars();
	return EngFuncs::GetCvarFloat( "czm_setup" ) == 0.0f;
}

void CZM_FinishFirstStart( void )
{
	CZM_RegisterSettingsCvars();
	EngFuncs::CvarSetValue( "czm_setup", 1.0f );

	if( CZM_CurrentSpray() < 0 )
		CZM_SetSpray( 0 );

	EngFuncs::ClientCmd( false, "host_writeconfig\n" );
}

void CZM_ApplyStartupSettings( void )
{
	CZM_RegisterSettingsCvars();

	// the client does not save the zoom sensitivity, we keep our own copy
	EngFuncs::CvarSetValue( "zoom_sensitivity_ratio", EngFuncs::GetCvarFloat( "czm_zoom_sens" ));

	// settings players no longer see: smooth network play and server downloads
	if( EngFuncs::GetCvarFloat( "czm_defaults" ) < 1.0f )
	{
		EngFuncs::CvarSetValue( "rate", 100000.0f );
		EngFuncs::CvarSetValue( "cl_updaterate", 60.0f );
		EngFuncs::CvarSetValue( "cl_cmdrate", 60.0f );
		EngFuncs::CvarSetValue( "czm_defaults", 1.0f );
	}

	EngFuncs::CvarSetValue( "cl_allowdownload", 1.0f );

	CZM_TouchStartup();
}

/* Public game-connection identity shared by the menu and TAB. GPL-3.0-or-later. */
#pragma once
#include <cstdio>
#include <cstring>

namespace CzmTab {
enum { MAX_SLOTS = 32 };
struct Player { int userid; char tag[33]; };
struct State { char server[64]; int self; Player players[MAX_SLOTS + 1]; };

inline bool Tag( const char *text )
{
	return text && std::strlen( text ) == 32 && std::strspn( text, "0123456789abcdef" ) == 32;
}

inline bool Parse( const char *text, State &out )
{
	std::memset( &out, 0, sizeof( out ));
	if( !text || !*text ) return false;
	char line[128], tail;
	const char *end = std::strchr( text, '\n' );
	if( !end || end - text >= (int)sizeof( line )) return false;
	std::memcpy( line, text, end - text ); line[end - text] = 0;
	if( std::sscanf( line, "%63s %d %c", out.server, &out.self, &tail ) != 2 || out.self <= 0
		|| std::strspn( out.server, "0123456789abcdefABCDEF.:[]" ) != std::strlen( out.server )) return false;
	text = end + 1;
	while( *text )
	{
		end = std::strchr( text, '\n' );
		if( !end || end - text >= (int)sizeof( line )) return false;
		std::memcpy( line, text, end - text ); line[end - text] = 0;
		int slot, userid; char tag[33];
		if( std::sscanf( line, "%d %d %32s %c", &slot, &userid, tag, &tail ) != 3
			|| slot < 1 || slot > MAX_SLOTS || userid <= 0 || out.players[slot].userid
			|| ( std::strcmp( tag, "-" ) && !Tag( tag ))) return false;
		out.players[slot].userid = userid;
		if( Tag( tag )) std::strcpy( out.players[slot].tag, tag );
		text = end + 1;
	}
	return true;
}

inline void Prefix( char *out, size_t size, const State &state, int slot )
{
	std::snprintf( out, size, "%s|%d|%s|", state.server, state.players[slot].userid, state.players[slot].tag );
}

inline bool AvatarPath( const char *path )
{
	if( !path ) return false;
	if( !std::strcmp( path, "gfx/czm/ui/avatar.png" ) || !std::strcmp( path, "gfx/czm/ui/icon_user.png" )) return true;
	int index; char tail;
	if( std::sscanf( path, "gfx/czm/avatars/avatar_%02d.png%c", &index, &tail ) == 1 && index >= 1 && index <= 10 )
	{
		char expected[64]; std::snprintf( expected, sizeof( expected ), "gfx/czm/avatars/avatar_%02d.png", index );
		return !std::strcmp( path, expected );
	}
	const char *number = NULL;
	if( !std::strncmp( path, "czm_avatar_", 11 )) number = path + 11;
	else if( !std::strncmp( path, "czm_cache/av_", 13 )) number = path + 13;
	else if( !std::strncmp( path, "czm_cache/account_", 18 )) number = path + 18;
	if( !number || !*number || *number == '_' || std::strlen( path ) >= 64 ) return false;
	size_t count = std::strspn( number, "0123456789_" );
	return count > 0 && !std::strcmp( number + count, ".png" );
}
}

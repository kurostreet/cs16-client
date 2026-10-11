/*
CzmJson.cpp - Counter Zombie Mobile: a small JSON reader for the answers of our service
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

#include "CzmJson.h"

#define CZM_JSON_DEPTH 32

void CzmJson::SkipSpace( const char *&p )
{
	while( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' )
		p++;
}

static void PutUtf8( CUtlVector<char> &out, unsigned int c )
{
	if( c < 0x80 )
		out.AddToTail( (char)c );
	else if( c < 0x800 )
	{
		out.AddToTail( (char)( 0xC0 | ( c >> 6 )));
		out.AddToTail( (char)( 0x80 | ( c & 0x3F )));
	}
	else if( c < 0x10000 )
	{
		out.AddToTail( (char)( 0xE0 | ( c >> 12 )));
		out.AddToTail( (char)( 0x80 | (( c >> 6 ) & 0x3F )));
		out.AddToTail( (char)( 0x80 | ( c & 0x3F )));
	}
	else
	{
		out.AddToTail( (char)( 0xF0 | ( c >> 18 )));
		out.AddToTail( (char)( 0x80 | (( c >> 12 ) & 0x3F )));
		out.AddToTail( (char)( 0x80 | (( c >> 6 ) & 0x3F )));
		out.AddToTail( (char)( 0x80 | ( c & 0x3F )));
	}
}

static int HexDigit( char c )
{
	if( c >= '0' && c <= '9' ) return c - '0';
	if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
	if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
	return -1;
}

static bool Hex4( const char *p, unsigned int *out )
{
	unsigned int v = 0;

	for( int i = 0; i < 4; i++ )
	{
		int d = HexDigit( p[i] );
		if( d < 0 )
			return false;
		v = v * 16 + d;
	}

	*out = v;
	return true;
}

bool CzmJson::ParseString( const char *&p, int *offset )
{
	if( *p != '"' )
		return false;
	p++;

	*offset = m_text.Count();
	while( *p && *p != '"' )
	{
		if( *p != '\\' )
		{
			m_text.AddToTail( *p++ );
			continue;
		}

		p++;
		switch( *p )
		{
		case '"': case '\\': case '/': m_text.AddToTail( *p ); break;
		case 'b': m_text.AddToTail( '\b' ); break;
		case 'f': m_text.AddToTail( '\f' ); break;
		case 'n': m_text.AddToTail( '\n' ); break;
		case 'r': m_text.AddToTail( '\r' ); break;
		case 't': m_text.AddToTail( '\t' ); break;
		case 'u':
		{
			unsigned int c, low;

			if( !Hex4( p + 1, &c ))
				return false;
			p += 4;
			// a surrogate pair is one character
			if( c >= 0xD800 && c <= 0xDBFF && p[1] == '\\' && p[2] == 'u' && Hex4( p + 3, &low ) && low >= 0xDC00 && low <= 0xDFFF )
			{
				c = 0x10000 + (( c - 0xD800 ) << 10 ) + ( low - 0xDC00 );
				p += 6;
			}
			PutUtf8( m_text, c );
			break;
		}
		default:
			return false;
		}
		p++;
	}

	if( *p != '"' )
		return false;
	p++;
	m_text.AddToTail( 0 );
	return true;
}

int CzmJson::ParseValue( const char *&p, int depth )
{
	Node node;
	int index;

	if( depth > CZM_JSON_DEPTH )
		return -1;

	SkipSpace( p );
	memset( &node, 0, sizeof( node ));
	node.key = node.first = node.next = node.str = -1;
	index = m_nodes.AddToTail( node );

	if( *p == '{' || *p == '[' )
	{
		bool object = *p == '{';
		char close = object ? '}' : ']';
		int last = -1;

		m_nodes[index].type = object ? T_OBJECT : T_ARRAY;
		p++;
		SkipSpace( p );
		if( *p == close )
		{
			p++;
			return index;
		}

		while( true )
		{
			int key = -1, child;

			SkipSpace( p );
			if( object )
			{
				if( !ParseString( p, &key ))
					return -1;
				SkipSpace( p );
				if( *p != ':' )
					return -1;
				p++;
			}

			child = ParseValue( p, depth + 1 );
			if( child < 0 )
				return -1;
			m_nodes[child].key = key;

			if( last < 0 )
				m_nodes[index].first = child;
			else
				m_nodes[last].next = child;
			last = child;
			m_nodes[index].count++;

			SkipSpace( p );
			if( *p == ',' )
			{
				p++;
				continue;
			}
			if( *p == close )
			{
				p++;
				return index;
			}
			return -1;
		}
	}

	if( *p == '"' )
	{
		int offset;

		if( !ParseString( p, &offset ))
			return -1;
		m_nodes[index].type = T_STRING;
		m_nodes[index].str = offset;
		return index;
	}

	if( !strncmp( p, "true", 4 ) || !strncmp( p, "false", 5 ))
	{
		m_nodes[index].type = T_BOOL;
		m_nodes[index].number = *p == 't' ? 1.0 : 0.0;
		p += *p == 't' ? 4 : 5;
		return index;
	}

	if( !strncmp( p, "null", 4 ))
	{
		p += 4;
		return index;
	}

	if( *p == '-' || ( *p >= '0' && *p <= '9' ))
	{
		char *end;

		m_nodes[index].type = T_NUMBER;
		m_nodes[index].number = strtod( p, &end );
		if( end == p )
			return -1;
		p = end;
		return index;
	}

	return -1;
}

bool CzmJson::Parse( const char *text )
{
	const char *p = text ? text : "";

	m_nodes.RemoveAll();
	m_text.RemoveAll();
	if( ParseValue( p, 0 ) != 0 )
	{
		m_nodes.RemoveAll();
		return false;
	}
	return true;
}

int CzmJson::Get( int object, const char *key ) const
{
	if( Type( object ) != T_OBJECT )
		return -1;

	for( int child = m_nodes[object].first; child >= 0; child = m_nodes[child].next )
	{
		if( m_nodes[child].key >= 0 && !strcmp( &m_text[m_nodes[child].key], key ))
			return child;
	}

	return -1;
}

int CzmJson::Count( int array ) const
{
	int type = Type( array );
	return ( type == T_ARRAY || type == T_OBJECT ) ? m_nodes[array].count : 0;
}

int CzmJson::At( int array, int index ) const
{
	if( Type( array ) != T_ARRAY )
		return -1;

	for( int child = m_nodes[array].first; child >= 0; child = m_nodes[child].next, index-- )
	{
		if( index == 0 )
			return child;
	}

	return -1;
}

double CzmJson::Number( int node, double def ) const
{
	if( !Valid( node ))
		return def;
	if( m_nodes[node].type == T_NUMBER || m_nodes[node].type == T_BOOL )
		return m_nodes[node].number;
	if( m_nodes[node].type == T_STRING )
		return atof( &m_text[m_nodes[node].str] );
	return def;
}

const char *CzmJson::String( int node, const char *def ) const
{
	if( !Valid( node ) || m_nodes[node].type != T_STRING )
		return def;
	return &m_text[m_nodes[node].str];
}

void CZM_JsonQuote( CUtlString &out, const char *text )
{
	out.Append( "\"" );
	for( const unsigned char *p = (const unsigned char *)( text ? text : "" ); *p; p++ )
	{
		switch( *p )
		{
		case '"': out.Append( "\\\"" ); break;
		case '\\': out.Append( "\\\\" ); break;
		case '\n': out.Append( "\\n" ); break;
		case '\r': out.Append( "\\r" ); break;
		case '\t': out.Append( "\\t" ); break;
		default:
			if( *p < 0x20 )
				out.AppendFormat( "\\u%04x", *p );
			else
			{
				char c[2] = { (char)*p, 0 };
				out.Append( c );
			}
			break;
		}
	}
	out.Append( "\"" );
}

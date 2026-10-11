/*
CzmJson.h - Counter Zombie Mobile: a small JSON reader for the answers of our service
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
#ifndef CZM_JSON_H
#define CZM_JSON_H

#include "utlvector.h"
#include "utlstring.h"

/*
Nodes are numbers: 0 is the root, -1 is "missing". Reading a missing node gives the
default, so chains like j.Int( j.Get( j.Get( 0, "ban" ), "until" )) need no checks.
*/
class CzmJson
{
public:
	enum { T_NULL = 0, T_BOOL, T_NUMBER, T_STRING, T_ARRAY, T_OBJECT };

	bool Parse( const char *text );

	int Type( int node ) const { return Valid( node ) ? m_nodes[node].type : T_NULL; }
	int Get( int object, const char *key ) const;
	int Count( int array ) const;
	int At( int array, int index ) const;

	double Number( int node, double def = 0.0 ) const;
	int Int( int node, int def = 0 ) const { return (int)Number( node, def ); }
	const char *String( int node, const char *def = "" ) const;
	bool Bool( int node ) const { return Valid( node ) && m_nodes[node].type == T_BOOL && m_nodes[node].number != 0.0; }
	bool IsNull( int node ) const { return Type( node ) == T_NULL; }

private:
	struct Node
	{
		int type;
		double number;
		int str;    // offset in m_text for strings and object keys
		int key;    // offset of the key when the node is an object member, -1 otherwise
		int first;  // first child of arrays and objects
		int next;   // next sibling
		int count;
	};

	bool Valid( int node ) const { return node >= 0 && node < m_nodes.Count(); }
	int ParseValue( const char *&p, int depth );
	bool ParseString( const char *&p, int *offset );
	void SkipSpace( const char *&p );

	CUtlVector<Node> m_nodes;
	CUtlVector<char> m_text;
};

// JSON string with quotes for a request body
void CZM_JsonQuote( CUtlString &out, const char *text );

#endif // CZM_JSON_H

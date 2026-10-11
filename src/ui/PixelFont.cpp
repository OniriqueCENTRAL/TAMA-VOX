#include "../StdCommon.h"
#include "PixelFont.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace tama::ui::font
{
namespace
{
// Format : ":<caractère> <ligne de départ>" puis des lignes de 5 colonnes ('#' allumé, '.' éteint).
// Capitales = lignes 0..6 ; minuscules = lignes 2..6 (jambages jusqu'à la ligne 8).
const char* kSpec = R"FONT(
:A 0
.###.
#...#
#...#
#####
#...#
#...#
#...#
:B 0
####.
#...#
#...#
####.
#...#
#...#
####.
:C 0
.###.
#...#
#....
#....
#....
#...#
.###.
:D 0
####.
#...#
#...#
#...#
#...#
#...#
####.
:E 0
#####
#....
#....
####.
#....
#....
#####
:F 0
#####
#....
#....
####.
#....
#....
#....
:G 0
.###.
#...#
#....
#.###
#...#
#...#
.####
:H 0
#...#
#...#
#...#
#####
#...#
#...#
#...#
:I 0
.###.
..#..
..#..
..#..
..#..
..#..
.###.
:J 0
..###
...#.
...#.
...#.
...#.
#..#.
.##..
:K 0
#...#
#..#.
#.#..
##...
#.#..
#..#.
#...#
:L 0
#....
#....
#....
#....
#....
#....
#####
:M 0
#...#
##.##
#.#.#
#.#.#
#...#
#...#
#...#
:N 0
#...#
##..#
#.#.#
#..##
#...#
#...#
#...#
:O 0
.###.
#...#
#...#
#...#
#...#
#...#
.###.
:P 0
####.
#...#
#...#
####.
#....
#....
#....
:Q 0
.###.
#...#
#...#
#...#
#.#.#
#..#.
.##.#
:R 0
####.
#...#
#...#
####.
#.#..
#..#.
#...#
:S 0
.####
#....
#....
.###.
....#
....#
####.
:T 0
#####
..#..
..#..
..#..
..#..
..#..
..#..
:U 0
#...#
#...#
#...#
#...#
#...#
#...#
.###.
:V 0
#...#
#...#
#...#
#...#
#...#
.#.#.
..#..
:W 0
#...#
#...#
#...#
#.#.#
#.#.#
##.##
#...#
:X 0
#...#
#...#
.#.#.
..#..
.#.#.
#...#
#...#
:Y 0
#...#
#...#
.#.#.
..#..
..#..
..#..
..#..
:Z 0
#####
....#
...#.
..#..
.#...
#....
#####
:a 2
.###.
....#
.####
#...#
.####
:b 0
#....
#....
#.##.
##..#
#...#
##..#
#.##.
:c 2
.###.
#...#
#....
#...#
.###.
:d 0
....#
....#
.##.#
#..##
#...#
#..##
.##.#
:e 2
.###.
#...#
#####
#....
.###.
:f 0
..##.
.#..#
.#...
###..
.#...
.#...
.#...
:g 2
.####
#...#
#...#
.####
....#
#...#
.###.
:h 0
#....
#....
#.##.
##..#
#...#
#...#
#...#
:i 0
..#..
.....
.##..
..#..
..#..
..#..
.###.
:j 0
...#.
.....
..##.
...#.
...#.
...#.
...#.
#..#.
.##..
:k 0
#....
#....
#..#.
#.#..
##...
#.#..
#..#.
:l 0
.##..
..#..
..#..
..#..
..#..
..#..
.###.
:m 2
##.#.
#.#.#
#.#.#
#...#
#...#
:n 2
#.##.
##..#
#...#
#...#
#...#
:o 2
.###.
#...#
#...#
#...#
.###.
:p 2
#.##.
##..#
#...#
##..#
#.##.
#....
#....
:q 2
.##.#
#..##
#...#
#..##
.##.#
....#
....#
:r 2
#.##.
##..#
#....
#....
#....
:s 2
.####
#....
.###.
....#
####.
:t 0
.#...
.#...
###..
.#...
.#...
.#..#
..##.
:u 2
#...#
#...#
#...#
#..##
.##.#
:v 2
#...#
#...#
#...#
.#.#.
..#..
:w 2
#...#
#...#
#.#.#
#.#.#
.#.#.
:x 2
#...#
.#.#.
..#..
.#.#.
#...#
:y 2
#...#
#...#
#...#
.####
....#
#...#
.###.
:z 2
#####
...#.
..#..
.#...
#####
:0 0
.###.
#...#
#..##
#.#.#
##..#
#...#
.###.
:1 0
..#..
.##..
..#..
..#..
..#..
..#..
.###.
:2 0
.###.
#...#
....#
...#.
..#..
.#...
#####
:3 0
.###.
#...#
....#
..##.
....#
#...#
.###.
:4 0
...#.
..##.
.#.#.
#..#.
#####
...#.
...#.
:5 0
#####
#....
####.
....#
....#
#...#
.###.
:6 0
..##.
.#...
#....
####.
#...#
#...#
.###.
:7 0
#####
....#
...#.
..#..
.#...
.#...
.#...
:8 0
.###.
#...#
#...#
.###.
#...#
#...#
.###.
:9 0
.###.
#...#
#...#
.####
....#
...#.
.##..
:! 0
..#..
..#..
..#..
..#..
..#..
.....
..#..
:" 0
.#.#.
.#.#.
.....
:# 0
.#.#.
#####
.#.#.
.#.#.
.#.#.
#####
.#.#.
:$ 0
..#..
.####
#.#..
.###.
..#.#
####.
..#..
:% 0
##..#
##..#
...#.
..#..
.#...
#..##
#..##
:& 0
.##..
#..#.
#.#..
.#...
#.#.#
#..#.
.##.#
:' 0
..#..
..#..
.#...
:( 0
...#.
..#..
.#...
.#...
.#...
..#..
...#.
:) 0
.#...
..#..
...#.
...#.
...#.
..#..
.#...
:* 0
.....
..#..
#.#.#
.###.
#.#.#
..#..
.....
:+ 0
.....
..#..
..#..
#####
..#..
..#..
.....
:, 5
..##.
..#..
.#...
:- 3
#####
:. 5
.##..
.##..
:/ 0
....#
....#
...#.
..#..
.#...
#....
#....
:: 0
.....
..##.
..##.
.....
..##.
..##.
.....
:; 0
.....
..##.
..##.
.....
..##.
..#..
.#...
:< 0
...#.
..#..
.#...
#....
.#...
..#..
...#.
:= 2
#####
.....
#####
:> 0
.#...
..#..
...#.
....#
...#.
..#..
.#...
:? 0
.###.
#...#
....#
...#.
..#..
.....
..#..
:@ 0
.###.
#...#
#.###
#.#.#
#.###
#....
.###.
:[ 0
.###.
.#...
.#...
.#...
.#...
.#...
.###.
:\ 0
#....
#....
.#...
..#..
...#.
....#
....#
:] 0
.###.
...#.
...#.
...#.
...#.
...#.
.###.
:^ 0
..#..
.#.#.
#...#
:_ 8
#####
:` 0
.#...
..#..
:{ 0
...##
..#..
..#..
.#...
..#..
..#..
...##
:| 0
..#..
..#..
..#..
..#..
..#..
..#..
..#..
:} 0
##...
..#..
..#..
...#.
..#..
..#..
##...
:~ 2
.##.#
#.##.
:· 3
..#..
)FONT";

struct Table
{
	uint8_t rows[128][9] {};
	bool have[128] {};
	Table ()
	{
		const char* s = kSpec;
		int ch = -1, row = 0;
		while (*s)
		{
			const char* e = std::strchr (s, '\n');
			const size_t len = e ? static_cast<size_t> (e - s) : std::strlen (s);
			if (len > 0)
			{
				if (s[0] == ':')
				{
					// ':' + caractère (ASCII ou « · » en UTF-8, stocké en 127) + ' ' + ligne de départ
					size_t k = 1;
					if (static_cast<unsigned char> (s[1]) == 0xC2) { ch = 127; k = 3; } else { ch = static_cast<unsigned char> (s[1]); k = 2; }
					row = std::atoi (std::string (s + k, len - k).c_str ());
					if (ch >= 0 && ch < 128) have[ch] = true;
				}
				else if (ch >= 0 && row < 9)
				{
					uint8_t m = 0;
					for (int i = 0; i < 5 && i < static_cast<int> (len); ++i) if (s[i] == '#') m = static_cast<uint8_t> (m | (1 << (4 - i)));
					rows[ch][row++] = m;
				}
			}
			if (!e) break;
			s = e + 1;
		}
	}
};
const Table& table () { static const Table t; return t; }

struct Glyph { int base; int mark; }; // mark : 0 aucune, 1 aigu, 2 grave, 3 circonflexe, 4 tréma, 5 cédille
Glyph decode (const char*& s, const char* end)
{
	const unsigned char c = static_cast<unsigned char> (*s);
	if (c < 0x80) { ++s; return {c, 0}; }
	if (c == 0xC3 && s + 1 < end)
	{
		const unsigned char d = static_cast<unsigned char> (s[1]);
		s += 2;
		switch (d)
		{
			case 0xA9: return {'e', 1}; case 0xA8: return {'e', 2}; case 0xAA: return {'e', 3}; case 0xAB: return {'e', 4};
			case 0xA0: return {'a', 2}; case 0xA2: return {'a', 3}; case 0xA4: return {'a', 4};
			case 0xB9: return {'u', 2}; case 0xBB: return {'u', 3}; case 0xBC: return {'u', 4};
			case 0xB4: return {'o', 3}; case 0xB6: return {'o', 4};
			case 0xAE: return {'i', 3}; case 0xAF: return {'i', 4};
			case 0xA7: return {'c', 5};
			case 0x89: return {'E', 1}; case 0x88: return {'E', 2}; case 0x80: return {'A', 2}; case 0x87: return {'C', 5};
			default: return {'?', 0};
		}
	}
	if (c == 0xC2 && s + 1 < end && static_cast<unsigned char> (s[1]) == 0xB7) { s += 2; return {127, 0}; }
	if (c == 0xE2 && s + 2 < end) // … → ↔ ✓
	{
		const unsigned char d = static_cast<unsigned char> (s[1]), e = static_cast<unsigned char> (s[2]);
		s += 3;
		if (d == 0x80 && e == 0xA6) return {128, 0}; // …  (rendu comme trois points)
		if (d == 0x86 && e == 0x94) return {129, 0}; // ↔
		if (d == 0x86 && e == 0x92) return {'>', 0}; // →
		return {'?', 0};
	}
	++s;
	while (s < end && (static_cast<unsigned char> (*s) & 0xC0) == 0x80) ++s;
	return {'?', 0};
}

void drawGlyphPixels (Painter& p, int base, int mark, double x, double y, int sc, Color col)
{
	const Table& t = table ();
	if (base >= 128 || !t.have[base]) return;
	const uint8_t* r = t.rows[base];
	const bool upper = base >= 'A' && base <= 'Z';
	// horizontal runs
	for (int j = 0; j < 9; ++j)
	{
		int i = 0;
		while (i < 5)
		{
			if (r[j] & (1 << (4 - i)))
			{
				int k = i;
				while (k < 5 && (r[j] & (1 << (4 - k)))) ++k;
				p.rect (x + i * sc, y + j * sc, (k - i) * sc, sc, col);
				i = k;
			}
			else
				++i;
		}
	}
	auto dot = [&] (int cx, int cy, int w = 1) { p.rect (x + cx * sc, y + cy * sc, w * sc, sc, col); };
	const int ay = upper ? -1 : 0; // ligne de l'accent
	switch (mark)
	{
		case 1: dot (3, ay); dot (2, ay + 1); break;
		case 2: dot (1, ay); dot (2, ay + 1); break;
		case 3: dot (2, ay); dot (1, ay + 1); dot (3, ay + 1); break;
		case 4: dot (1, ay + (upper ? 0 : 0)); dot (3, ay); break;
		case 5: dot (2, 7); dot (1, 8); break;
		default: break;
	}
}
} // namespace

int textLines (std::string_view s)
{
	int n = 1;
	for (char c : s) if (c == '\n') ++n;
	return n;
}

static int lineWidthChars (const char* s, const char* end)
{
	int n = 0;
	while (s < end) { const Glyph g = decode (s, end); n += (g.base == 128) ? 3 : (g.base == 129 ? 2 : 1); }
	return n;
}

int textWidth (std::string_view s, int scale)
{
	int best = 0;
	const char* b = s.data (); const char* end = b + s.size ();
	const char* ls = b;
	for (const char* q = b; q <= end; ++q)
		if (q == end || *q == '\n') { best = std::max (best, lineWidthChars (ls, q)); ls = q + 1; }
	return best > 0 ? (best * kAdvance - 1) * scale : 0;
}

void drawText (Painter& p, std::string_view s, double x, double y, int scale, Color c, Align a)
{
	const char* b = s.data (); const char* end = b + s.size ();
	const char* ls = b;
	double ly = y;
	for (const char* q = b; q <= end; ++q)
	{
		if (q != end && *q != '\n') continue;
		const int chars = lineWidthChars (ls, q);
		const double w = chars > 0 ? (chars * kAdvance - 1) * scale : 0;
		double cx = a == Align::Left ? x : (a == Align::Centre ? x - w * 0.5 : x - w);
		cx = std::floor (cx);
		const char* t = ls;
		while (t < q)
		{
			const Glyph g = decode (t, q);
			if (g.base == 128) { for (int k = 0; k < 3; ++k) { p.rect (cx + 1 * scale, ly + 6 * scale, scale, scale, c); cx += kAdvance * scale; } }
			else if (g.base == 129) { drawGlyphPixels (p, '<', 0, cx, ly, scale, c); cx += kAdvance * scale; drawGlyphPixels (p, '>', 0, cx, ly, scale, c); cx += kAdvance * scale; }
			else { drawGlyphPixels (p, g.base, g.mark, cx, ly, scale, c); cx += kAdvance * scale; }
		}
		ly += kLineHeight * scale;
		ls = q + 1;
	}
}

std::string wrap (std::string_view s, int maxChars)
{
	std::string out;
	const char* b = s.data (); const char* end = b + s.size ();
	const char* ls = b;
	for (const char* q = b; q <= end; ++q)
	{
		if (q != end && *q != '\n') continue;
		// mots de la ligne [ls, q)
		int col = 0;
		const char* w = ls;
		while (w < q)
		{
			const char* we = w;
			while (we < q && *we != ' ') ++we;
			const int wl = lineWidthChars (w, we);
			if (col > 0 && col + 1 + wl > maxChars) { out += '\n'; col = 0; }
			else if (col > 0) { out += ' '; ++col; }
			out.append (w, static_cast<size_t> (we - w));
			col += wl;
			w = we < q ? we + 1 : we;
		}
		if (q != end) out += '\n';
		ls = q + 1;
	}
	return out;
}
} // namespace tama::ui::font

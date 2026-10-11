#include "../StdCommon.h"
#include "Lcd.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace tama::ui
{
namespace
{
constexpr int W = kLcdW, H = kLcdH;
constexpr int NONE = -1;

struct Rng
{
	uint32_t s;
	uint32_t next () { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	int below (int n) { return static_cast<int> (next () % static_cast<uint32_t> (n)); }
	double unit () { return (next () & 0xFFFFFF) / 16777216.0; }
};

struct Canvas
{
	uint8_t* b;
	Rng rng;

	void put (int x, int y, int c)
	{
		if (x < 0 || y < 0 || x >= W || y >= H || c == NONE) return;
		uint8_t& d = b[y * W + x];
		if (c == kLcdInk || c == kLcdBg) { d = static_cast<uint8_t> (c); return; }
		// buée / ombre : ne recouvre jamais l'encre, l'ombre recouvre la buée
		if (d == kLcdInk) return;
		if (c == kLcdGhost && d == kLcdShade) return;
		d = static_cast<uint8_t> (c);
	}
	void px (double x, double y, int w, int h, int c)
	{
		const int ix = static_cast<int> (std::floor (x + 0.5)), iy = static_cast<int> (std::floor (y + 0.5));
		for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i) put (ix + i, iy + j, c);
	}
	void bmp (const char* const* rows, int n, int x, int y, int c)
	{
		for (int j = 0; j < n; ++j) for (int i = 0; rows[j][i]; ++i) if (rows[j][i] == '#') put (x + i, y + j, c);
	}
	void ell (double cx, double cy, double rx, double ry, int fill, int stroke)
	{
		auto ins = [&] (int x, int y) { const double a = (x + .5 - cx) / rx, bb = (y + .5 - cy) / ry; return a * a + bb * bb <= 1; };
		for (int y = static_cast<int> (std::floor (cy - ry - 1)); y <= static_cast<int> (std::ceil (cy + ry + 1)); ++y)
			for (int x = static_cast<int> (std::floor (cx - rx - 1)); x <= static_cast<int> (std::ceil (cx + rx + 1)); ++x)
			{
				if (!ins (x, y)) continue;
				const bool edge = !ins (x - 1, y) || !ins (x + 1, y) || !ins (x, y - 1) || !ins (x, y + 1);
				if (edge && stroke != NONE) put (x, y, stroke); else if (fill != NONE) put (x, y, fill);
			}
	}
	void tri (double ax, double ay, double bx, double by, double cx, double cy, int c)
	{
		const int x0 = static_cast<int> (std::floor (std::min ({ax, bx, cx}))), x1 = static_cast<int> (std::ceil (std::max ({ax, bx, cx})));
		const int y0 = static_cast<int> (std::floor (std::min ({ay, by, cy}))), y1 = static_cast<int> (std::ceil (std::max ({ay, by, cy})));
		const double d = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
		if (d == 0) return;
		for (int y = y0; y <= y1; ++y)
			for (int x = x0; x <= x1; ++x)
			{
				const double qx = x + .5, qy = y + .5;
				const double u = ((qx - ax) * (cy - ay) - (cx - ax) * (qy - ay)) / d, v = ((bx - ax) * (qy - ay) - (qx - ax) * (by - ay)) / d;
				if (u >= 0 && v >= 0 && u + v <= 1) put (x, y, c);
			}
	}
};

const char* const kBolt[] = {"....##..", "...##...", "..##....", ".######.", "...##...", "..##....", ".##....."};
const char* const kSq[] = {"########", "..####..", "...##...", "........", "...##...", "..####..", "########"};
const char* const kEss[] = {".#####..", "##...##.", "##......", ".####...", "....##..", "##...##.", ".#####.."};
const char* const kHeart[] = {".##..##.", "########", "########", "########", ".######.", "..####..", "...##..."};
const char* const kPlay[] = {"##......", "####....", "######..", "########", "######..", "####....", "##......"};
const char* const kDia[] = {"...##...", "..####..", ".######.", "########", ".######.", "..####..", "...##..."};
const char* const kRaw[] = {"..####..", ".#....#.", "#....#.#", "#...#..#", "#..#...#", ".#....#.", "..####.."};
const char* const kWarn[] = {"...##...", "...##...", "...##...", "...##...", "........", "...##...", "...##..."};
const char* const kTubeBody[] = {"...###...", "..#...#..", ".#.....#.", "#.......#", "#.......#", "#.......#", "#.......#", "#.......#",
                                 "#.......#", "#.......#", "#.......#", "#.......#", "#.......#", ".#######.", ".#.#.#.#.", ".#######."};

void tube (Canvas& c, int x, int y, double g)
{
	c.bmp (kTubeBody, 16, x, y, kLcdInk);
	const int wires[2][3] = {{3, 4, 11}, {5, 4, 11}};
	for (int i = 0; i < 2; ++i)
	{
		const double need = i ? .38 : .14;
		for (int r = wires[i][1]; r <= wires[i][2]; ++r)
			c.px (x + wires[i][0], y + r, 1, 1, (g > need && (g > .75 || c.rng.unit () < .35 + g * .65)) ? kLcdInk : kLcdGhost);
	}
	c.px (x + 3, y + 3, 3, 1, g > .55 ? kLcdInk : kLcdGhost);
	c.px (x + 3, y + 12, 3, 1, g > .55 ? kLcdInk : kLcdGhost);
	if (g > .6) c.px (x + 4, y + 7, 1, 1, kLcdInk);
	if (g > .82)
	{
		c.px (x - 2, y + 6, 1, 1, kLcdInk); c.px (x + 10, y + 6, 1, 1, kLcdInk); c.px (x + 4, y - 2, 1, 1, kLcdInk);
		c.px (x - 1, y + 1, 1, 1, kLcdInk); c.px (x + 9, y + 1, 1, 1, kLcdInk);
	}
}

void hand (Canvas& c, double hx, double hy, double k, int dir)
{
	const int rx = static_cast<int> (std::floor (3 + 3 * k + .5)), ry = static_cast<int> (std::floor (3 + 2 * k + .5));
	if (k > .5) c.ell (hx, hy, rx + 3, ry + 4, kLcdGhost, NONE);
	if (k > .45)
	{
		const int lens[4] = {5, 7, 7, 6};
		const int pitch = std::max (2, static_cast<int> (std::floor (2.0 * rx / 4 + .5)));
		for (int i = 0; i < 4; ++i)
		{
			const double lx0 = hx - rx + i * pitch;
			const int len = static_cast<int> (std::floor (lens[i] * (.4 + .6 * k) + .5));
			const double top = hy - ry - len + 1;
			c.px (lx0, top, 1, len + 1, kLcdInk); c.px (lx0 + pitch, top, 1, len + 1, kLcdInk); c.px (lx0, top, pitch + 1, 1, kLcdInk);
			c.px (lx0 + 1, top + 1, pitch - 1, len, kLcdBg);
		}
	}
	c.ell (hx, hy, rx, ry, kLcdBg, kLcdInk);
	if (k > .45) c.ell (hx - dir * (rx + 1), hy - 1, 2, 3, kLcdBg, kLcdInk);
	if (k > .45) c.px (hx - 1, hy + 1, 3, 1, kLcdShade);
}

struct Mood { bool sleep, sad, ess; };

void catgirl (Canvas& c, double now, double p, const Mood& st, const LcdState& s)
{
	const double t = now / 1000.0, sc = .95 + .25 * p;
	const int cx = 32, cy = static_cast<int> (std::floor (34 - 3 * p + .5));
	const int hw = static_cast<int> (std::floor (11 * sc * (1 + .2 * p) + .5)), hh = static_cast<int> (std::floor (9.5 * sc * (1 - .1 * p) + .5));
	const bool lampHot = s.lamp > .55;
	if (p > .3) c.ell (cx, cy + 1, hw + 7, hh + 6, kLcdGhost, NONE);
	// queue
	if (p < .45 && !st.sleep)
	{
		for (int i = 0; i < 16; ++i)
		{
			const int x = cx + hw + 3 + static_cast<int> (std::floor (5 * std::sin (i * .38 + t * 2.2) + i * .25 + .5)), y = 47 - static_cast<int> (std::floor (i * 1.3 + .5));
			c.px (x, y, 2, 2, kLcdInk);
		}
		const int tx = cx + hw + 3 + static_cast<int> (std::floor (5 * std::sin (15 * .38 + t * 2.2) + 15 * .25 + .5));
		c.px (tx - 1, 47 - 20, 4, 3, kLcdInk);
	}
	// corps : col marin, grelot
	if (p < .85)
	{
		const int y0 = cy + hh + 1;
		for (int y = y0; y <= 49; ++y)
		{
			const int half = static_cast<int> (std::floor (hw * .8 + (y - y0) * .55 + .5));
			for (int x = cx - half; x <= cx + half; ++x)
				c.put (x, y, (x == cx - half || x == cx + half || y == 49) ? kLcdInk : (y < y0 + 3 ? kLcdInk : kLcdBg));
		}
		c.px (cx - 1, y0, 3, 3, kLcdBg); c.px (cx, y0 + 1, 1, 1, kLcdInk); c.px (cx - 3, y0 + 5, 7, 1, kLcdShade);
	}
	// cheveux, mèches
	c.ell (cx, cy - 1, hw + 2, hh + 2, kLcdInk, NONE);
	const int lockLen = hh + (p < .5 ? 6 : 1);
	for (int sd : {-1, 1}) { const int x0 = sd < 0 ? cx - hw - 2 : cx + hw; c.px (x0, cy, 3, lockLen, kLcdInk); }
	// oreilles de chat
	const int droop = st.sad ? 4 : 0, twitch = (std::fmod (now, 4000.0) < 120) ? 1 : 0;
	const double flat = p * 3;
	for (int sd : {-1, 1})
	{
		const double bx0 = cx + sd * (hw - 1), bx1 = cx + sd * std::floor (hw * .22 + .5), by = cy - hh + 4;
		const double ax = cx + sd * (std::floor (hw * .78 + .5) + flat + droop), ay = by - std::floor (13 * sc + .5) + std::floor (p * 3 + .5) + droop + (sd > 0 ? twitch : 0);
		c.tri (bx0, by, bx1, by, ax, ay, kLcdInk);
		const double mx = (bx0 + bx1 + ax) / 3, my = (by + by + ay) / 3, sh = .5;
		c.tri (mx + (bx0 - mx) * sh, my + (by - my) * sh + 1, mx + (bx1 - mx) * sh, my + (by - my) * sh + 1, mx + (ax - mx) * sh, my + (ay - my) * sh + 1, kLcdBg);
	}
	// visage
	c.ell (cx, cy + 1, hw - 1, hh, kLcdBg, kLcdInk);
	// frange en dents de scie
	for (int y = cy + 1 - hh; y <= cy + 1 + hh; ++y)
		for (int x = cx - hw; x <= cx + hw; ++x)
		{
			const double a = (x + .5 - cx) / (hw - 1), bb = (y + .5 - cy - 1) / hh;
			if (a * a + bb * bb > 1) continue;
			const int tw = std::abs (((x - cx + 60) % 6) - 3);
			const int line = cy - static_cast<int> (std::floor (hh * .45 + .5)) + static_cast<int> (std::floor (tw * .8 + .5)) + (std::abs (x - cx) < 2 ? -2 : 0);
			if (y < line) c.put (x, y, kLcdInk);
		}
	// yeux
	const int ex = static_cast<int> (std::floor (hw * .5 + .5)), ey = cy + static_cast<int> (std::floor (hh * .12 + .5));
	const int ew = std::max (4, static_cast<int> (std::floor (5 * sc + .5))), eh = std::max (5, static_cast<int> (std::floor (6.5 * sc + .5)));
	const bool blink = !st.sleep && (std::fmod (now, 3200.0) < 140);
	for (int sd : {-1, 1})
	{
		const int x = cx + sd * ex - (ew >> 1);
		if (st.sleep || blink)
		{
			c.px (x - 1, ey + 1, ew + 2, 1, kLcdInk); c.px (x - 1, ey, 1, 1, kLcdInk); c.px (x + ew, ey, 1, 1, kLcdInk);
		}
		else if (p > .8)
		{
			for (int i = 0; i < 3; ++i)
			{
				c.px(sd < 0 ? x + i : x + ew - 1 - i, ey - 1 + i, 1, 1, kLcdInk);
				c.px(sd < 0 ? x + i : x + ew - 1 - i, ey + 3 - i, 1, 1, kLcdInk);
			}
			c.px (sd < 0 ? x + 3 : x + ew - 4, ey + 1, 1, 1, kLcdInk);
		}
		else if (p > .45)
		{
			c.px (x - 1, ey - 1, ew + 2, 1, kLcdInk); c.px (x - 1, ey + eh, ew + 2, 1, kLcdInk);
			c.px (x - 1, ey, 1, eh, kLcdInk); c.px (x + ew, ey, 1, eh, kLcdInk);
			c.px (x + (ew >> 1) - (sd > 0 ? 0 : 1), ey + (eh >> 1) - 1, 2, 2, kLcdInk);
		}
		else
		{
			c.px (x, ey - 1, ew, eh + 1, kLcdInk);
			c.px (x + 1, ey, 2, 2, kLcdBg); c.px (x + ew - 2, ey + eh - 2, 1, 1, kLcdBg);
			c.px (sd < 0 ? x - 1 : x + ew, ey - 1, 1, 1, kLcdInk);
			if (st.sad) c.px (x + (ew >> 1), ey + eh + 1, 1, 3, kLcdShade);
		}
		if (st.sad) c.px (sd < 0 ? x : x + ew - 3, ey - 3, 3, 1, kLcdInk);
	}
	// museau et bouche
	const int ny = cy + static_cast<int> (std::floor (hh * .62 + .5));
	if (p > .5)
	{
		c.px (cx - 2, ny - 1, 5, 2, kLcdInk);
		const int lw = 4 + static_cast<int> (std::floor (p * 3 + .5));
		c.px (cx - lw, ny + 2, 2 * lw + 1, 1, kLcdInk); c.px (cx - lw, ny + 5, 2 * lw + 1, 1, kLcdInk);
		c.px (cx - lw - 1, ny + 3, 1, 2, kLcdInk); c.px (cx + lw + 1, ny + 3, 1, 2, kLcdInk);
		c.px (cx - lw, ny + 3, 2 * lw + 1, 2, kLcdShade);
	}
	else if (st.ess)
	{
		c.px (cx, ny - 1, 1, 1, kLcdInk); c.px (cx - 2, ny + 1, 5, 3, kLcdInk); c.px (cx - 2, ny + 1, 1, 1, kLcdBg); c.px (cx + 2, ny + 1, 1, 1, kLcdBg);
		c.px (cx - 1, ny + 3, 3, 1, kLcdShade);
		c.px (cx + 4, ny + 1, 4, 1, kLcdInk); c.px (cx + 5, ny + 3, 4, 1, kLcdInk); c.px (cx - 8, ny + 1, 4, 1, kLcdInk); c.px (cx - 9, ny + 3, 4, 1, kLcdInk);
	}
	else if (st.sleep) { c.px (cx, ny, 1, 1, kLcdInk); c.px (cx - 1, ny + 2, 3, 1, kLcdInk); }
	else if (st.sad) { c.px (cx, ny, 1, 1, kLcdInk); c.px (cx - 2, ny + 3, 1, 1, kLcdInk); c.px (cx - 1, ny + 2, 3, 1, kLcdInk); c.px (cx + 2, ny + 3, 1, 1, kLcdInk); }
	else
	{
		c.px (cx, ny, 1, 1, kLcdInk); c.px (cx - 2, ny + 1, 1, 1, kLcdInk); c.px (cx, ny + 1, 1, 1, kLcdInk); c.px (cx + 2, ny + 1, 1, 1, kLcdInk);
		c.px (cx - 1, ny + 2, 1, 1, kLcdInk); c.px (cx + 1, ny + 2, 1, 1, kLcdInk);
	}
	// joues et moustaches
	const int bl = lampHot ? 3 : 2;
	for (int sd : {-1, 1})
	{
		const int bx = cx + sd * static_cast<int> (std::floor (hw * .68 + .5)) - 2;
		if (!st.sleep) for (int r = 0; r < bl; ++r) c.px (bx, ey + eh + 1 + r, 5, 1, r == 1 ? kLcdShade : kLcdGhost);
		const int wx = sd < 0 ? cx - hw + 1 : cx + hw - 3;
		c.px (wx, ny - 1 + (sd < 0 ? 0 : 1), 3, 1, kLcdInk); c.px (wx, ny + 2 - (sd < 0 ? 0 : 1), 3, 1, kLcdInk);
	}
	// mains
	const double k = std::clamp (p * 1.6 - .1, 0.0, 1.0);
	const int lyr = static_cast<int> (std::floor (46 - (46 - (cy + 9)) * k + .5)), ryr = static_cast<int> (std::floor (46 - (46 - (cy - 3)) * k + .5));
	const int lxr = static_cast<int> (std::floor (cx - hw - 3 - 5 * k + .5)), rxr = static_cast<int> (std::floor (cx + hw + 3 + 5 * k + .5));
	const int lx = std::max (7, lxr), rx = std::min (56, rxr);
	hand (c, lx, lyr, st.sleep ? 0 : k, -1);
	hand (c, rx, ryr, st.sleep ? 0 : k, 1);
	if (p > .7)
	{
		const int hs[2][2] = {{lx, lyr}, {rx, ryr}};
		for (auto& h : hs)
		{
			c.px (h[0] - 9, h[1] - 5, 2, 1, kLcdInk); c.px (h[0] + 8, h[1] - 5, 2, 1, kLcdInk);
			c.px (h[0] - 10, h[1] + 1, 2, 1, kLcdInk); c.px (h[0] + 9, h[1] + 1, 2, 1, kLcdInk);
		}
	}
	if (st.sleep)
	{
		const int zx = cx + hw + 1, zy = std::max (10, cy - hh - 8);
		static const char* const z1[] = {"####", "...#", "..#.", ".#..", "####"};
		static const char* const z2[] = {"###", "..#", ".#.", "###"};
		if (static_cast<long> (now / 700) % 2) c.bmp (z1, 5, zx, zy, kLcdInk); else c.bmp (z2, 4, zx + 1, zy + 1, kLcdInk);
	}
}
} // namespace

void renderLcd (uint8_t* out, const LcdState& s, double nowMs, uint32_t seed)
{
	std::memset (out, kLcdBg, static_cast<size_t> (W * H));
	Canvas c {out, Rng {seed ? seed : 0x9E3779B9u}};
	// fond ondulé
	for (int y = 10; y < 51; ++y)
		for (int x = 0; x < W; ++x)
			if (std::sin (x * .3 + std::sin (y * .22) * 3) > .82) c.put (x, y, kLcdGhost);
	const double g = std::clamp ((s.playing ? .14 : .05) + .86 * s.lamp, 0.0, 1.0);
	struct Icon { const char* const* rows; bool on; };
	const Icon top[4] = {{kBolt, s.lamp > .45}, {kSq, s.grDb > 3 && !s.raw}, {kEss, s.duckDb > 3.0}, {kHeart, s.autoGain}};
	const Icon bot[4] = {{kPlay, s.playing}, {kDia, s.pure}, {kRaw, s.raw}, {kWarn, s.tooLoud && s.playing}};
	for (int i = 0; i < 4; ++i)
	{
		c.bmp (top[i].rows, 7, 4 + i * 16, 1, top[i].on ? kLcdInk : kLcdGhost);
		c.bmp (bot[i].rows, 7, 4 + i * 16, 52, bot[i].on ? kLcdInk : kLcdGhost);
	}
	tube (c, 2, 10, g);
	const bool sleeping = !s.playing;
	const double p = (s.raw || sleeping) ? 0.0 : s.squash;
	catgirl (c, nowMs, p, {sleeping, s.raw && s.playing, s.duckDb > 3.0 && p < .5}, s);
	for (int x = 6; x < 58; x += 2) c.put (x, 51, kLcdGhost);
	// parasites sur les transitoires saturées
	if (s.glitch > 0)
	{
		const int n = 3 + c.rng.below (4);
		for (int i = 0; i < n; ++i)
		{
			const int y = 10 + c.rng.below (40), h = 1 + c.rng.below (3), dx = (c.rng.below (2) ? 1 : -1) * (2 + c.rng.below (8));
			uint8_t row[W];
			for (int j = 0; j < h && y + j < H; ++j)
			{
				std::memcpy (row, out + (y + j) * W, W);
				for (int x = 0; x < W; ++x) { const int sx = x - dx; if (sx >= 0 && sx < W) out[(y + j) * W + x] = row[sx]; }
			}
		}
		for (int k = 0; k < 16; ++k) c.put (c.rng.below (W), c.rng.below (H), c.rng.below (2) ? kLcdInk : kLcdBg);
	}
}
} // namespace tama::ui

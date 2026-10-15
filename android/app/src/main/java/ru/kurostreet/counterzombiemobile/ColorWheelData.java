/* Exact HSV selection shared by the touch picker and its host checks. GPL-3.0-or-later. */
package ru.kurostreet.counterzombiemobile;

final class ColorWheelData {
    private ColorWheelData() {}
    static float unit(float value) { return Math.max(0, Math.min(1, value)); }
    static float hue(float value) { return ((value % 360) + 360) % 360; }
    static float[] point(float x, float y, float radius) {
        return new float[] {hue((float)Math.toDegrees(Math.atan2(y, x))),
            radius <= 0 ? 0 : unit((float)Math.hypot(x, y) / radius)};
    }
    static int rgb(float h, float s, float v) {
        h = hue(h) / 60; s = unit(s); v = unit(v);
        float c = v * s, x = c * (1 - Math.abs(h % 2 - 1)), m = v - c;
        float r = 0, g = 0, b = 0;
        switch ((int)h) {
            case 0: r = c; g = x; break;
            case 1: r = x; g = c; break;
            case 2: g = c; b = x; break;
            case 3: g = x; b = c; break;
            case 4: r = x; b = c; break;
            default: r = c; b = x; break;
        }
        return 0xff000000 | Math.round((r + m) * 255) << 16 | Math.round((g + m) * 255) << 8 | Math.round((b + m) * 255);
    }
    static float[] hsv(int r, int g, int b) {
        float rf = unit(r / 255f), gf = unit(g / 255f), bf = unit(b / 255f);
        float max = Math.max(rf, Math.max(gf, bf)), min = Math.min(rf, Math.min(gf, bf)), d = max - min, h = 0;
        if (d > 0) {
            if (max == rf) h = 60 * ((gf - bf) / d);
            else if (max == gf) h = 60 * ((bf - rf) / d + 2);
            else h = 60 * ((rf - gf) / d + 4);
        }
        return new float[] {hue(h), max == 0 ? 0 : d / max, max};
    }
}

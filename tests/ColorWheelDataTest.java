package ru.kurostreet.counterzombiemobile;

public final class ColorWheelDataTest {
    static void equal(int a, int b) { if (a != b) throw new AssertionError(Integer.toHexString(a) + " != " + Integer.toHexString(b)); }
    static void near(float a, float b) { if (Math.abs(a - b) > .001f) throw new AssertionError(a + " != " + b); }
    public static void main(String[] args) {
        equal(ColorWheelData.rgb(0, 1, 1), 0xffff0000);
        equal(ColorWheelData.rgb(60, 1, 1), 0xffffff00);
        equal(ColorWheelData.rgb(120, 1, 1), 0xff00ff00);
        equal(ColorWheelData.rgb(180, 1, 1), 0xff00ffff);
        equal(ColorWheelData.rgb(240, 1, 1), 0xff0000ff);
        equal(ColorWheelData.rgb(300, 1, 1), 0xffff00ff);
        equal(ColorWheelData.rgb(-60, 1, 1), 0xffff00ff);
        equal(ColorWheelData.rgb(720, 0, 1), 0xffffffff);
        equal(ColorWheelData.rgb(123, 1, 0), 0xff000000);
        equal(ColorWheelData.rgb(0, 2, 2), 0xffff0000);
        near(ColorWheelData.point(10, 0, 10)[0], 0);
        near(ColorWheelData.point(0, 10, 10)[0], 90);
        near(ColorWheelData.point(-10, 0, 10)[0], 180);
        near(ColorWheelData.point(0, -10, 10)[0], 270);
        near(ColorWheelData.point(100, 0, 10)[1], 1);
        near(ColorWheelData.point(0, 0, 10)[1], 0);
        near(ColorWheelData.point(4, 3, 10)[1], .5f);
        near(ColorWheelData.point(10, 0, 0)[1], 0);
        for (int r = 0; r <= 255; r += 17) for (int g = 0; g <= 255; g += 17) for (int b = 0; b <= 255; b += 17) {
            float[] hsv = ColorWheelData.hsv(r, g, b);
            equal(ColorWheelData.rgb(hsv[0], hsv[1], hsv[2]), 0xff000000 | r << 16 | g << 8 | b);
        }
        System.out.println("COLOR_WHEEL_OK");
    }
}

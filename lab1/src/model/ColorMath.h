#pragma once

namespace ColorMath {

struct RGB { double r = 0.0, g = 0.0, b = 0.0; };

struct CMYK { double c = 0.0, m = 0.0, y = 0.0, k = 0.0; };

struct HLS { double h = 0.0, l = 0.0, s = 0.0; };

enum class CmykAlgorithm { GCR, UCR };
enum class GamutStrategy { Clipping, Scaling };

double clampValue(double v, double lo, double hi);

RGB fitToGamut(RGB c, GamutStrategy strategy, bool& wasOutOfGamut);

HLS rgbToHls(const RGB& c);
RGB hlsToRgb(const HLS& c, GamutStrategy strategy, bool& wasOutOfGamut);

CMYK rgbToCmyk(const RGB& c, CmykAlgorithm algorithm);
RGB cmykToRgb(const CMYK& c, GamutStrategy strategy, bool& wasOutOfGamut);

}
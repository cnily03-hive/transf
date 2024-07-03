/** -*- C++ -*-
* This code is based on simple_term_colors by illyigan
* @see https://github.com/illyigan/simple_term_colors
//===----------------------------------------------------------------------===//
// MIT License

// Copyright (c) 2024 illyigan

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//===----------------------------------------------------------------------===//
*/

#include <ostream>
#include <sstream>

//===----------------------------------------------------------------------===//
// Following starts the stc namespace.
// Functions in this namespace provides utilities for ansi color codes.
//===----------------------------------------------------------------------===//
namespace stc {

enum ColorModes { COLOR_256 = 0, TRUE_COLOR = 1, NO_COLOR = 2 };

inline int _get_color_mode_index() {
    static const int i = std::ios_base::xalloc();
    return i;
}

class color_data {
   public:
    unsigned int r : 8, g : 8, b : 8, code : 8;
    constexpr color_data(int r, int g, int b, int code) : r(r), g(g), b(b), code(code) {};
};

template <bool IS_FOREGROUND>
class color_code : public color_data {
    using color_data::color_data;
};

// foreground

inline std::ostream &operator<<(std::ostream &os, const color_code<true> &color_code) {
    const long MODE = os.iword(_get_color_mode_index());
    if (MODE == ColorModes::COLOR_256)
        os << "\033[38;5;" << color_code.code << 'm';
    else if (MODE == ColorModes::TRUE_COLOR)
        os << "\033[38;2;" << color_code.r << ';' << color_code.g << ';' << color_code.b << 'm';
    return os;
}

// background

inline std::ostream &operator<<(std::ostream &os, const color_code<false> &color_code) {
    const long MODE = os.iword(_get_color_mode_index());
    if (MODE == ColorModes::COLOR_256)
        os << "\033[48;5;" << color_code.code << 'm';
    else if (MODE == ColorModes::TRUE_COLOR)
        os << "\033[48;2;" << color_code.r << ';' << color_code.g << ';' << color_code.b << 'm';
    return os;
}

constexpr const color_data MAP_256C[256] = {
    color_data{0, 0, 0, 0 /*Black(SYSTEM)*/},
    color_data{128, 0, 0, 1 /*Maroon(SYSTEM)*/},
    color_data{0, 128, 0, 2 /*Green(SYSTEM)*/},
    color_data{128, 128, 0, 3 /*Olive(SYSTEM)*/},
    color_data{0, 0, 128, 4 /*Navy(SYSTEM)*/},
    color_data{128, 0, 128, 5 /*Purple(SYSTEM)*/},
    color_data{0, 128, 128, 6 /*Teal(SYSTEM)*/},
    color_data{192, 192, 192, 7 /*Silver(SYSTEM)*/},
    color_data{128, 128, 128, 8 /*Grey(SYSTEM)*/},
    color_data{255, 0, 0, 9 /*Red(SYSTEM)*/},
    color_data{0, 255, 0, 10 /*Lime(SYSTEM)*/},
    color_data{255, 255, 0, 11 /*Yellow(SYSTEM)*/},
    color_data{0, 0, 255, 12 /*Blue(SYSTEM)*/},
    color_data{255, 0, 255, 13 /*Fuchsia(SYSTEM)*/},
    color_data{0, 255, 255, 14 /*Aqua(SYSTEM)*/},
    color_data{255, 255, 255, 15 /*White(SYSTEM)*/},
    color_data{0, 0, 0, 16 /*Grey0*/},
    color_data{0, 0, 95, 17 /*NavyBlue*/},
    color_data{0, 0, 135, 18 /*DarkBlue*/},
    color_data{0, 0, 175, 19 /*Blue3*/},
    color_data{0, 0, 215, 20 /*Blue3*/},
    color_data{0, 0, 255, 21 /*Blue1*/},
    color_data{0, 95, 0, 22 /*DarkGreen*/},
    color_data{0, 95, 95, 23 /*DeepSkyBlue4*/},
    color_data{0, 95, 135, 24 /*DeepSkyBlue4*/},
    color_data{0, 95, 175, 25 /*DeepSkyBlue4*/},
    color_data{0, 95, 215, 26 /*DodgerBlue3*/},
    color_data{0, 95, 255, 27 /*DodgerBlue2*/},
    color_data{0, 135, 0, 28 /*Green4*/},
    color_data{0, 135, 95, 29 /*SpringGreen4*/},
    color_data{0, 135, 135, 30 /*Turquoise4*/},
    color_data{0, 135, 175, 31 /*DeepSkyBlue3*/},
    color_data{0, 135, 215, 32 /*DeepSkyBlue3*/},
    color_data{0, 135, 255, 33 /*DodgerBlue1*/},
    color_data{0, 175, 0, 34 /*Green3*/},
    color_data{0, 175, 95, 35 /*SpringGreen3*/},
    color_data{0, 175, 135, 36 /*DarkCyan*/},
    color_data{0, 175, 175, 37 /*LightSeaGreen*/},
    color_data{0, 175, 215, 38 /*DeepSkyBlue2*/},
    color_data{0, 175, 255, 39 /*DeepSkyBlue1*/},
    color_data{0, 215, 0, 40 /*Green3*/},
    color_data{0, 215, 95, 41 /*SpringGreen3*/},
    color_data{0, 215, 135, 42 /*SpringGreen2*/},
    color_data{0, 215, 175, 43 /*Cyan3*/},
    color_data{0, 215, 215, 44 /*DarkTurquoise*/},
    color_data{0, 215, 255, 45 /*Turquoise2*/},
    color_data{0, 255, 0, 46 /*Green1*/},
    color_data{0, 255, 95, 47 /*SpringGreen2*/},
    color_data{0, 255, 135, 48 /*SpringGreen1*/},
    color_data{0, 255, 175, 49 /*MediumSpringGreen*/},
    color_data{0, 255, 215, 50 /*Cyan2*/},
    color_data{0, 255, 255, 51 /*Cyan1*/},
    color_data{95, 0, 0, 52 /*DarkRed*/},
    color_data{95, 0, 95, 53 /*DeepPink4*/},
    color_data{95, 0, 135, 54 /*Purple4*/},
    color_data{95, 0, 175, 55 /*Purple4*/},
    color_data{95, 0, 215, 56 /*Purple3*/},
    color_data{95, 0, 255, 57 /*BlueViolet*/},
    color_data{95, 95, 0, 58 /*Orange4*/},
    color_data{95, 95, 95, 59 /*Grey37*/},
    color_data{95, 95, 135, 60 /*MediumPurple4*/},
    color_data{95, 95, 175, 61 /*SlateBlue3*/},
    color_data{95, 95, 215, 62 /*SlateBlue3*/},
    color_data{95, 95, 255, 63 /*RoyalBlue1*/},
    color_data{95, 135, 0, 64 /*Chartreuse4*/},
    color_data{95, 135, 95, 65 /*DarkSeaGreen4*/},
    color_data{95, 135, 135, 66 /*PaleTurquoise4*/},
    color_data{95, 135, 175, 67 /*SteelBlue*/},
    color_data{95, 135, 215, 68 /*SteelBlue3*/},
    color_data{95, 135, 255, 69 /*CornflowerBlue*/},
    color_data{95, 175, 0, 70 /*Chartreuse3*/},
    color_data{95, 175, 95, 71 /*DarkSeaGreen4*/},
    color_data{95, 175, 135, 72 /*CadetBlue*/},
    color_data{95, 175, 175, 73 /*CadetBlue*/},
    color_data{95, 175, 215, 74 /*SkyBlue3*/},
    color_data{95, 175, 255, 75 /*SteelBlue1*/},
    color_data{95, 215, 0, 76 /*Chartreuse3*/},
    color_data{95, 215, 95, 77 /*PaleGreen3*/},
    color_data{95, 215, 135, 78 /*SeaGreen3*/},
    color_data{95, 215, 175, 79 /*Aquamarine3*/},
    color_data{95, 215, 215, 80 /*MediumTurquoise*/},
    color_data{95, 215, 255, 81 /*SteelBlue1*/},
    color_data{95, 255, 0, 82 /*Chartreuse2*/},
    color_data{95, 255, 95, 83 /*SeaGreen2*/},
    color_data{95, 255, 135, 84 /*SeaGreen1*/},
    color_data{95, 255, 175, 85 /*SeaGreen1*/},
    color_data{95, 255, 215, 86 /*Aquamarine1*/},
    color_data{95, 255, 255, 87 /*DarkSlateGray2*/},
    color_data{135, 0, 0, 88 /*DarkRed*/},
    color_data{135, 0, 95, 89 /*DeepPink4*/},
    color_data{135, 0, 135, 90 /*DarkMagenta*/},
    color_data{135, 0, 175, 91 /*DarkMagenta*/},
    color_data{135, 0, 215, 92 /*DarkViolet*/},
    color_data{135, 0, 255, 93 /*Purple*/},
    color_data{135, 95, 0, 94 /*Orange4*/},
    color_data{135, 95, 95, 95 /*LightPink4*/},
    color_data{135, 95, 135, 96 /*Plum4*/},
    color_data{135, 95, 175, 97 /*MediumPurple3*/},
    color_data{135, 95, 215, 98 /*MediumPurple3*/},
    color_data{135, 95, 255, 99 /*SlateBlue1*/},
    color_data{135, 135, 0, 100 /*Yellow4*/},
    color_data{135, 135, 95, 101 /*Wheat4*/},
    color_data{135, 135, 135, 102 /*Grey53*/},
    color_data{135, 135, 175, 103 /*LightSlateGrey*/},
    color_data{135, 135, 215, 104 /*MediumPurple*/},
    color_data{135, 135, 255, 105 /*LightSlateBlue*/},
    color_data{135, 175, 0, 106 /*Yellow4*/},
    color_data{135, 175, 95, 107 /*DarkOliveGreen3*/},
    color_data{135, 175, 135, 108 /*DarkSeaGreen*/},
    color_data{135, 175, 175, 109 /*LightSkyBlue3*/},
    color_data{135, 175, 215, 110 /*LightSkyBlue3*/},
    color_data{135, 175, 255, 111 /*SkyBlue2*/},
    color_data{135, 215, 0, 112 /*Chartreuse2*/},
    color_data{135, 215, 95, 113 /*DarkOliveGreen3*/},
    color_data{135, 215, 135, 114 /*PaleGreen3*/},
    color_data{135, 215, 175, 115 /*DarkSeaGreen3*/},
    color_data{135, 215, 215, 116 /*DarkSlateGray3*/},
    color_data{135, 215, 255, 117 /*SkyBlue1*/},
    color_data{135, 255, 0, 118 /*Chartreuse1*/},
    color_data{135, 255, 95, 119 /*LightGreen*/},
    color_data{135, 255, 135, 120 /*LightGreen*/},
    color_data{135, 255, 175, 121 /*PaleGreen1*/},
    color_data{135, 255, 215, 122 /*Aquamarine1*/},
    color_data{135, 255, 255, 123 /*DarkSlateGray1*/},
    color_data{175, 0, 0, 124 /*Red3*/},
    color_data{175, 0, 95, 125 /*DeepPink4*/},
    color_data{175, 0, 135, 126 /*MediumVioletRed*/},
    color_data{175, 0, 175, 127 /*Magenta3*/},
    color_data{175, 0, 215, 128 /*DarkViolet*/},
    color_data{175, 0, 255, 129 /*Purple*/},
    color_data{175, 95, 0, 130 /*DarkOrange3*/},
    color_data{175, 95, 95, 131 /*IndianRed*/},
    color_data{175, 95, 135, 132 /*HotPink3*/},
    color_data{175, 95, 175, 133 /*MediumOrchid3*/},
    color_data{175, 95, 215, 134 /*MediumOrchid*/},
    color_data{175, 95, 255, 135 /*MediumPurple2*/},
    color_data{175, 135, 0, 136 /*DarkGoldenrod*/},
    color_data{175, 135, 95, 137 /*LightSalmon3*/},
    color_data{175, 135, 135, 138 /*RosyBrown*/},
    color_data{175, 135, 175, 139 /*Grey63*/},
    color_data{175, 135, 215, 140 /*MediumPurple2*/},
    color_data{175, 135, 255, 141 /*MediumPurple1*/},
    color_data{175, 175, 0, 142 /*Gold3*/},
    color_data{175, 175, 95, 143 /*DarkKhaki*/},
    color_data{175, 175, 135, 144 /*NavajoWhite3*/},
    color_data{175, 175, 175, 145 /*Grey69*/},
    color_data{175, 175, 215, 146 /*LightSteelBlue3*/},
    color_data{175, 175, 255, 147 /*LightSteelBlue*/},
    color_data{175, 215, 0, 148 /*Yellow3*/},
    color_data{175, 215, 95, 149 /*DarkOliveGreen3*/},
    color_data{175, 215, 135, 150 /*DarkSeaGreen3*/},
    color_data{175, 215, 175, 151 /*DarkSeaGreen2*/},
    color_data{175, 215, 215, 152 /*LightCyan3*/},
    color_data{175, 215, 255, 153 /*LightSkyBlue1*/},
    color_data{175, 255, 0, 154 /*GreenYellow*/},
    color_data{175, 255, 95, 155 /*DarkOliveGreen2*/},
    color_data{175, 255, 135, 156 /*PaleGreen1*/},
    color_data{175, 255, 175, 157 /*DarkSeaGreen2*/},
    color_data{175, 255, 215, 158 /*DarkSeaGreen1*/},
    color_data{175, 255, 255, 159 /*PaleTurquoise1*/},
    color_data{215, 0, 0, 160 /*Red3*/},
    color_data{215, 0, 95, 161 /*DeepPink3*/},
    color_data{215, 0, 135, 162 /*DeepPink3*/},
    color_data{215, 0, 175, 163 /*Magenta3*/},
    color_data{215, 0, 215, 164 /*Magenta3*/},
    color_data{215, 0, 255, 165 /*Magenta2*/},
    color_data{215, 95, 0, 166 /*DarkOrange3*/},
    color_data{215, 95, 95, 167 /*IndianRed*/},
    color_data{215, 95, 135, 168 /*HotPink3*/},
    color_data{215, 95, 175, 169 /*HotPink2*/},
    color_data{215, 95, 215, 170 /*Orchid*/},
    color_data{215, 95, 255, 171 /*MediumOrchid1*/},
    color_data{215, 135, 0, 172 /*Orange3*/},
    color_data{215, 135, 95, 173 /*LightSalmon3*/},
    color_data{215, 135, 135, 174 /*LightPink3*/},
    color_data{215, 135, 175, 175 /*Pink3*/},
    color_data{215, 135, 215, 176 /*Plum3*/},
    color_data{215, 135, 255, 177 /*Violet*/},
    color_data{215, 175, 0, 178 /*Gold3*/},
    color_data{215, 175, 95, 179 /*LightGoldenrod3*/},
    color_data{215, 175, 135, 180 /*Tan*/},
    color_data{215, 175, 175, 181 /*MistyRose3*/},
    color_data{215, 175, 215, 182 /*Thistle3*/},
    color_data{215, 175, 255, 183 /*Plum2*/},
    color_data{215, 215, 0, 184 /*Yellow3*/},
    color_data{215, 215, 95, 185 /*Khaki3*/},
    color_data{215, 215, 135, 186 /*LightGoldenrod2*/},
    color_data{215, 215, 175, 187 /*LightYellow3*/},
    color_data{215, 215, 215, 188 /*Grey84*/},
    color_data{215, 215, 255, 189 /*LightSteelBlue1*/},
    color_data{215, 255, 0, 190 /*Yellow2*/},
    color_data{215, 255, 95, 191 /*DarkOliveGreen1*/},
    color_data{215, 255, 135, 192 /*DarkOliveGreen1*/},
    color_data{215, 255, 175, 193 /*DarkSeaGreen1*/},
    color_data{215, 255, 215, 194 /*Honeydew2*/},
    color_data{215, 255, 255, 195 /*LightCyan1*/},
    color_data{255, 0, 0, 196 /*Red1*/},
    color_data{255, 0, 95, 197 /*DeepPink2*/},
    color_data{255, 0, 135, 198 /*DeepPink1*/},
    color_data{255, 0, 175, 199 /*DeepPink1*/},
    color_data{255, 0, 215, 200 /*Magenta2*/},
    color_data{255, 0, 255, 201 /*Magenta1*/},
    color_data{255, 95, 0, 202 /*OrangeRed1*/},
    color_data{255, 95, 95, 203 /*IndianRed1*/},
    color_data{255, 95, 135, 204 /*IndianRed1*/},
    color_data{255, 95, 175, 205 /*HotPink*/},
    color_data{255, 95, 215, 206 /*HotPink*/},
    color_data{255, 95, 255, 207 /*MediumOrchid1*/},
    color_data{255, 135, 0, 208 /*DarkOrange*/},
    color_data{255, 135, 95, 209 /*Salmon1*/},
    color_data{255, 135, 135, 210 /*LightCoral*/},
    color_data{255, 135, 175, 211 /*PaleVioletRed1*/},
    color_data{255, 135, 215, 212 /*Orchid2*/},
    color_data{255, 135, 255, 213 /*Orchid1*/},
    color_data{255, 175, 0, 214 /*Orange1*/},
    color_data{255, 175, 95, 215 /*SandyBrown*/},
    color_data{255, 175, 135, 216 /*LightSalmon1*/},
    color_data{255, 175, 175, 217 /*LightPink1*/},
    color_data{255, 175, 215, 218 /*Pink1*/},
    color_data{255, 175, 255, 219 /*Plum1*/},
    color_data{255, 215, 0, 220 /*Gold1*/},
    color_data{255, 215, 95, 221 /*LightGoldenrod2*/},
    color_data{255, 215, 135, 222 /*LightGoldenrod2*/},
    color_data{255, 215, 175, 223 /*NavajoWhite1*/},
    color_data{255, 215, 215, 224 /*MistyRose1*/},
    color_data{255, 215, 255, 225 /*Thistle1*/},
    color_data{255, 255, 0, 226 /*Yellow1*/},
    color_data{255, 255, 95, 227 /*LightGoldenrod1*/},
    color_data{255, 255, 135, 228 /*Khaki1*/},
    color_data{255, 255, 175, 229 /*Wheat1*/},
    color_data{255, 255, 215, 230 /*Cornsilk1*/},
    color_data{255, 255, 255, 231 /*Grey100*/},
    color_data{8, 8, 8, 232 /*Grey3*/},
    color_data{18, 18, 18, 233 /*Grey7*/},
    color_data{28, 28, 28, 234 /*Grey11*/},
    color_data{38, 38, 38, 235 /*Grey15*/},
    color_data{48, 48, 48, 236 /*Grey19*/},
    color_data{58, 58, 58, 237 /*Grey23*/},
    color_data{68, 68, 68, 238 /*Grey27*/},
    color_data{78, 78, 78, 239 /*Grey30*/},
    color_data{88, 88, 88, 240 /*Grey35*/},
    color_data{98, 98, 98, 241 /*Grey39*/},
    color_data{108, 108, 108, 242 /*Grey42*/},
    color_data{118, 118, 118, 243 /*Grey46*/},
    color_data{128, 128, 128, 244 /*Grey50*/},
    color_data{138, 138, 138, 245 /*Grey54*/},
    color_data{148, 148, 148, 246 /*Grey58*/},
    color_data{158, 158, 158, 247 /*Grey62*/},
    color_data{168, 168, 168, 248 /*Grey66*/},
    color_data{178, 178, 178, 249 /*Grey70*/},
    color_data{188, 188, 188, 250 /*Grey74*/},
    color_data{198, 198, 198, 251 /*Grey78*/},
    color_data{208, 208, 208, 252 /*Grey82*/},
    color_data{218, 218, 218, 253 /*Grey85*/},
    color_data{228, 228, 228, 254 /*Grey89*/},
    color_data{238, 238, 238, 255 /*Grey93*/},
};

constexpr float color_distance(int r, int g, int b, color_data color) {
    // approximate color distance using redmean
    // (https://en.wikipedia.org/wiki/Color_difference)
    const auto red_difference = (float)(color.r - r);
    const auto green_difference = (float)(color.g - g);
    const auto blue_difference = (float)(color.b - b);
    const float red_average = (float)(r + color.r) / 2.0F;
    return ((2 + (red_average / 256)) * (red_difference * red_difference)) +
           (4 * (green_difference * green_difference)) +
           ((2 + ((255 - red_average) / 256)) * (blue_difference * blue_difference));
}

constexpr int find_closest_color_code(int r, int g, int b) {
    // for dark colors we return black, the cutoff values are arbitrary but
    // prevent artifacting from redmean color distance approximation
    if (r < 20 && g < 15 && b < 15) return MAP_256C[16].code;
    // we start at index 16, because colors 0 - 16 are system colors (terminal
    // emulators often define custom values for these)
    size_t best_index = 16;
    for (size_t i = best_index; i < 256; i++) {
        if (color_distance(r, g, b, MAP_256C[i]) < color_distance(r, g, b, MAP_256C[best_index]))
            best_index = i;
    }
    return MAP_256C[best_index].code;
}

constexpr void hsl_to_rgb(float h, float s, float l, int &r, int &g, int &b) {
    auto fmod = [](float number, int divisor) {
        const int i = (int)number;
        return (float)(i % divisor) + (number - (float)i);
    };
    auto round = [](float number) {
        const int i = (int)number;
        if (number - (float)i >= 0.5) return i + 1;
        return i;
    };

    // (https://en.wikipedia.org/wiki/HSL_and_HSV#Color_conversion_formulae)
    if (s == 0) {
        r = g = b = round(l * 255);
        return;
    }
    const float alpha = s * std::min(l, 1 - l);
    auto f = [=](float n) {
        const float k = fmod((n + (h * 12)), 12);
        return l - (alpha * std::max(-1.0F, std::min({k - 3, 9 - k, 1.0F})));
    };
    r = round(f(0) * 255);
    g = round(f(8) * 255);
    b = round(f(4) * 255);
}

constexpr void clamp(int &a, int min, int max) {
    if (a < min)
        a = min;
    else if (a > max)
        a = max;
}

constexpr void clamp_rgb(int &r, int &g, int &b) {
    clamp(r, 0, 255);
    clamp(g, 0, 255);
    clamp(b, 0, 255);
}

inline std::ostream &print_if_color(std::ostream &os, std::string_view text) {
    const auto mode = os.iword(_get_color_mode_index());
    if (mode != ColorModes::NO_COLOR) return os << text;
    return os;
}

// input: a function (any), and its arguments
template <typename F, typename... Args>
inline std::string stream_to_string(F &&f, Args &&...args) {
    std::ostringstream oss;
    // get functoin call result and put it into the stream
    oss << std::forward<F>(f)(std::forward<Args>(args)...);
    return oss.str();
}
}  // namespace stc

/**
 * Based on utilites in stc namespace, ansi offers direct tool to use.
 */
namespace ansi {

//===----------------------------------------------------------------------===//
// Following namespace offers a more user-friendly functions compared to what in `stc`.
// Note that the return type of the functions should be passed to the stream.
//===----------------------------------------------------------------------===//
namespace stream {

// Sets the color mode to 256 color. (default)
inline std::ostream &color_256(std::ostream &os) {
    os.iword(stc::_get_color_mode_index()) = stc::ColorModes::COLOR_256;
    return os;
}

// Sets the color mode to true color.
inline std::ostream &true_color(std::ostream &os) {
    os.iword(stc::_get_color_mode_index()) = stc::ColorModes::TRUE_COLOR;
    return os;
}

/**
 * @brief Disables all color codes from being emitted to the stream.
 * @note If you set a style before, do not forget to use `ansi::reset` BEFORE `ansi::no_color`
 * as it will still be visible even after you change the color mode.
 * This mode simply guarantees no color codes will be printed, but it does not
 * erase already existing ones.
 */
inline std::ostream &no_color(std::ostream &os) {
    os.iword(stc::_get_color_mode_index()) = stc::ColorModes::NO_COLOR;
    return os;
}

// Resets the output style.
inline std::ostream &reset(std::ostream &os) { return stc::print_if_color(os, "\033[0m"); }
// Makes the text bold.
inline std::ostream &bold(std::ostream &os) { return stc::print_if_color(os, "\033[1m"); }
// Makes the text faint.
inline std::ostream &faint(std::ostream &os) { return stc::print_if_color(os, "\033[2m"); }
// Makes the text italic.
inline std::ostream &italic(std::ostream &os) { return stc::print_if_color(os, "\033[3m"); }
// Makes the text underlined.
inline std::ostream &underline(std::ostream &os) { return stc::print_if_color(os, "\033[4m"); }
// Swaps the background and foreground colors.
inline std::ostream &inverse(std::ostream &os) { return stc::print_if_color(os, "\033[7m"); }
// Makes the text crossed out.
inline std::ostream &crossed_out(std::ostream &os) { return stc::print_if_color(os, "\033[9m"); }
// Resets the output foreground color.
inline std::ostream &reset_fg(std::ostream &os) { return stc::print_if_color(os, "\033[39m"); }
// Resets the output background color.
inline std::ostream &reset_bg(std::ostream &os) { return stc::print_if_color(os, "\033[49m"); }

/**
 * @brief Sets the foreground color using RGB color model.
 * @param r,g,b are integers and should be in range 0-255.
 */
constexpr stc::color_code<true> rgb_fg(int r, int g, int b) {
    stc::clamp_rgb(r, g, b);
    return {r, g, b, stc::find_closest_color_code(r, g, b)};
}

/**
 * @brief Sets the background color using RGB color model.
 * @param r,g,b are integers and should be in range 0-255.
 */
constexpr stc::color_code<false> rgb_bg(int r, int g, int b) {
    stc::clamp_rgb(r, g, b);
    return {r, g, b, stc::find_closest_color_code(r, g, b)};
}

/**
 * @brief Sets the foreground color using HSL color model.
 * @param h,s,l are float values and should be in range 0-1.
 */
constexpr stc::color_code<true> hsl_fg(float h, float s, float l) {
    int r = 0, g = 0, b = 0;
    stc::hsl_to_rgb(h, s, l, r, g, b);
    stc::clamp_rgb(r, g, b);
    return {r, g, b, stc::find_closest_color_code(r, g, b)};
}

/**
 * @brief Sets the background color using HSL color model.
 * @param h,s,l are float values and should be in range 0-1.
 */
constexpr stc::color_code<false> hsl_bg(float h, float s, float l) {
    int r = 0, g = 0, b = 0;
    stc::hsl_to_rgb(h, s, l, r, g, b);
    stc::clamp_rgb(r, g, b);
    return {r, g, b, stc::find_closest_color_code(r, g, b)};
}

/**
 * @brief Sets the foreground color using a code.
 * @param code is an integer and should be in range 0-255.
 */
constexpr stc::color_code<true> code_fg(int code) {
    stc::clamp(code, 0, 255);
    const auto color_data = stc::MAP_256C[code];
    return {color_data.r, color_data.g, color_data.b, color_data.code};
}

/**
 * @brief Sets the background color using a code
 * @param code is an integer and should be in range 0-255.
 */
constexpr stc::color_code<false> code_bg(int code) {
    stc::clamp(code, 0, 255);
    const auto color_data = stc::MAP_256C[code];
    return {color_data.r, color_data.g, color_data.b, color_data.code};
}
}  // namespace stream

//===----------------------------------------------------------------------===//
// Common ansi escape sequences for text formatting,
// directly exposed in `ansi` namespace.
//===----------------------------------------------------------------------===//

// foreground colors

std::string black = "\033[0;30m";
std::string red = "\033[0;31m";
std::string green = "\033[0;32m";
std::string yellow = "\033[0;33m";
std::string blue = "\033[0;34m";
std::string magenta = "\033[0;35m";
std::string cyan = "\033[0;36m";
std::string white = "\033[0;37m";
std::string gray = "\033[0;90m";
std::string bright_red = "\033[0;91m";
std::string bright_green = "\033[0;92m";
std::string bright_yellow = "\033[0;93m";
std::string bright_blue = "\033[0;94m";
std::string bright_magenta = "\033[0;95m";
std::string bright_cyan = "\033[0;96m";
std::string bright_white = "\033[0;97m";

// background colors

std::string black_bg = "\033[40m";
std::string red_bg = "\033[41m";
std::string green_bg = "\033[42m";
std::string yellow_bg = "\033[43m";
std::string blue_bg = "\033[44m";
std::string magenta_bg = "\033[45m";
std::string cyan_bg = "\033[46m";
std::string white_bg = "\033[47m";
std::string gray_bg = "\033[100m";
std::string bright_red_bg = "\033[101m";
std::string bright_green_bg = "\033[102m";
std::string bright_yellow_bg = "\033[103m";
std::string bright_blue_bg = "\033[104m";
std::string bright_magenta_bg = "\033[105m";
std::string bright_cyan_bg = "\033[106m";
std::string bright_white_bg = "\033[107m";

// effects

std::string bold = "\033[1m";
std::string italic = "\033[3m";
std::string underline = "\033[4m";
std::string blink = "\033[5m";
std::string reverse = "\033[7m";
std::string invisible = "\033[8m";
std::string crossed_out = "\033[9m";

// control

std::string clear_screen = "\033[2J";
std::string clear_line = "\033[2K";
std::string clear_line_right = "\033[K";
std::string clear_line_left = "\033[1K";
std::string clear_down = "\033[J";
std::string clear_up = "\033[1J";
std::string cursor_home = "\033[H";
std::string cursor_pos_line_right = "\033[1000C";
std::string cursor_pos_x(int x) { return "\033[" + std::to_string(x) + "G"; }
std::string cursor_pos_y(int y) { return "\033[" + std::to_string(y) + "d"; }
std::string cursor_position(int x, int y) {
    return "\033[" + std::to_string(y) + ";" + std::to_string(x) + "H";
}
std::string cursor_up(int n) { return "\033[" + std::to_string(n) + "A"; }
std::string cursor_down(int n) { return "\033[" + std::to_string(n) + "B"; }
std::string cursor_forward(int n) { return "\033[" + std::to_string(n) + "C"; }
std::string cursor_back(int n) { return "\033[" + std::to_string(n) + "D"; }
std::string cursor_hide = "\033[?25l";
std::string cursor_show = "\033[?25h";
std::string save_cursor = "\033[s";
std::string restore_cursor = "\033[u";
std::string cursor_prev_line(int n) { return "\033[" + std::to_string(n) + "F"; }
std::string cursor_next_line(int n) { return "\033[" + std::to_string(n) + "E"; }

std::string reset = "\033[0m";
std::string reset_fg = "\033[39m";
std::string reset_bg = "\033[49m";

/**
 * @brief Generates a foreground color code using RGB color model.
 * @param r,g,b are integers and should be in range 0-255.
 */
std::string rgb_fg(int r, int g, int b) { return stc::stream_to_string(stream::rgb_fg, r, g, b); }

/**
 * @brief Generates a background color code using RGB color model.
 * @param r,g,b are integers and should be in range 0-255.
 */
std::string rgb_bg(int r, int g, int b) { return stc::stream_to_string(stream::rgb_bg, r, g, b); }

/**
 * @brief Generates a foreground color code using HSL color model.
 * @param h,s,l are float values and should be in range 0-1.
 */
std::string hsl_fg(float h, float s, float l) {
    return stc::stream_to_string(stream::hsl_fg, h, s, l);
}

/**
 * @brief Generates a background color code using HSL color model.
 * @param h,s,l are float values and should be in range 0-1.
 */
std::string hsl_bg(float h, float s, float l) {
    return stc::stream_to_string(stream::hsl_bg, h, s, l);
}

/**
 * @brief Generates a foreground color code using a code.
 * @param code is an integer and should be in range 0-255.
 */
std::string code_fg(int code) { return stc::stream_to_string(stream::code_fg, code); }

/**
 * @brief Generates a background color code using a code.
 * @param code is an integer and should be in range 0-255.
 */
std::string code_bg(int code) { return stc::stream_to_string(stream::code_bg, code); }

}  // namespace ansi
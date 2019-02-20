#pragma once

#include "runtime.h"

#define _CHI_COLOR_CSI(x)   CHI_IFELSE(CHI_COLOR_ENABLED, "\e[" x "m", "")
#define _CHI_COLOR_FG(x)    _CHI_COLOR_CSI("3" #x)
#define _CHI_COLOR_BG(x)    _CHI_COLOR_CSI("4" #x)

#define StyNone         _CHI_COLOR_CSI("0")
#define StyBold         _CHI_COLOR_CSI("1")
#define StyNobold       _CHI_COLOR_CSI("22")
#define StyUnder        _CHI_COLOR_CSI("4")
#define StyNounder      _CHI_COLOR_CSI("24")
#define FgBlack         _CHI_COLOR_FG(0)
#define FgRed           _CHI_COLOR_FG(1)
#define FgGreen         _CHI_COLOR_FG(2)
#define FgYellow        _CHI_COLOR_FG(3)
#define FgBlue          _CHI_COLOR_FG(4)
#define FgMagenta       _CHI_COLOR_FG(5)
#define FgCyan          _CHI_COLOR_FG(6)
#define FgWhite         _CHI_COLOR_FG(7)
#define FgDefault       _CHI_COLOR_FG(9)
#define BgBlack         _CHI_COLOR_BG(0)
#define BgRed           _CHI_COLOR_BG(1)
#define BgGreen         _CHI_COLOR_BG(2)
#define BgYellow        _CHI_COLOR_BG(3)
#define BgBlue          _CHI_COLOR_BG(4)
#define BgMagenta       _CHI_COLOR_BG(5)
#define BgCyan          _CHI_COLOR_BG(6)
#define BgWhite         _CHI_COLOR_BG(7)
#define BgDefault       _CHI_COLOR_BG(9)

#define TitleBegin      StyBold FgRed
#define TitleEnd        StyNobold FgDefault

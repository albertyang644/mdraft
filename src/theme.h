#ifndef THEME_H
#define THEME_H

#include <QString>

namespace Theme {
inline constexpr auto Accent = "#879fbd";
inline constexpr auto AccentDark = "#5a6b82";
inline constexpr auto LightPanel = "#e6ebf2";
inline constexpr auto LightPanelAlt = "#f3f6fa";
inline constexpr auto LightBorder = "#cdd7e4";
inline constexpr auto LightText = "#33404f";
inline constexpr auto DarkPanel = "#232323";
inline constexpr auto DarkWidget = "#2b2b2b";
inline constexpr auto DarkEditor = "#1e1e1e";
inline constexpr auto DarkBorder = "#3a3a3a";
inline constexpr auto DarkText = "#e0e0e0";
inline constexpr auto DarkTopText = "#e8edf5";
inline constexpr auto OutlineLight = "#4a5a70";
inline constexpr auto OutlineDark = "#c8d2e0";
inline constexpr auto CheckedDark = "#4a6fa5";
inline constexpr auto DirLight = "#eef2f7";
inline constexpr auto DirLightBorder = "#dde4ec";
inline constexpr auto DirLightText = "#5a6b82";
inline constexpr auto DirDarkText = "#9aa7b8";
inline constexpr auto CodeDarkBackground = "#30343a";
inline constexpr auto CodeDarkForeground = "#f0b38a";
inline constexpr auto EmphasisDark = "#e7b46a";
inline constexpr auto AccentBright = "#78b7ef";

QString darkApplicationStyleSheet();
QString markdownStyleSheet(bool dark);
}

#endif // THEME_H

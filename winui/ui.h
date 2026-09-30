// Small builders shared by the window, so the layout reads like the groups it describes.
//
// Colours come from WinUI's own theme resources rather than a hard-coded palette: they are the
// Windows look, they follow the system theme and accent colour, and they change with high
// contrast. The hand-drawn build had to define its own tokens and got them wrong twice.
#pragma once

#include "pch.h"

namespace SyncPlayer::ui {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

// A theme brush by resource key, transparent if the key is missing rather than throwing.
inline Media::Brush brush(hstring const& key) {
  auto res = Application::Current().Resources();
  if (res.HasKey(box_value(key))) {
    if (auto b = res.Lookup(box_value(key)).try_as<Media::Brush>()) return b;
  }
  return Media::SolidColorBrush(Windows::UI::Colors::Transparent());
}

inline Controls::TextBlock text(hstring const& value, double size = 14.0,
                               Media::Brush const& fg = nullptr, bool semibold = false) {
  Controls::TextBlock t;
  t.Text(value);
  t.FontSize(size);
  t.TextWrapping(TextWrapping::Wrap);
  t.Foreground(fg ? fg : brush(L"TextFillColorPrimaryBrush"));
  if (semibold) t.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
  return t;
}

// Secondary caption text: the helpers under controls and the group titles in the reference.
inline Controls::TextBlock caption(hstring const& value) {
  return text(value, 12.0, brush(L"TextFillColorSecondaryBrush"));
}

inline Controls::StackPanel row(double spacing = 8.0) {
  Controls::StackPanel p;
  p.Orientation(Orientation::Horizontal);
  p.Spacing(spacing);
  p.VerticalAlignment(VerticalAlignment::Center);
  return p;
}

inline Controls::StackPanel column(double spacing = 8.0) {
  Controls::StackPanel p;
  p.Orientation(Orientation::Vertical);
  p.Spacing(spacing);
  return p;
}

// A group card: rounded, card fill, the title in caps above the contents.
inline Controls::Border card(hstring const& title, Controls::StackPanel& out_body) {
  Controls::Border b;
  b.Background(brush(L"CardBackgroundFillColorDefaultBrush"));
  b.BorderBrush(brush(L"CardStrokeColorDefaultBrush"));
  b.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
  b.CornerRadius(CornerRadius{8.0, 8.0, 8.0, 8.0});
  b.Padding(Thickness{16.0, 12.0, 16.0, 16.0});

  auto body = column(8.0);
  auto title_block = text(title, 12.0, brush(L"TextFillColorSecondaryBrush"), true);
  title_block.CharacterSpacing(60);
  body.Children().Append(title_block);

  out_body = body;
  b.Child(body);
  return b;
}

// A caption above its control, the pattern the reference uses for Jump, Speed and the videos.
inline Controls::StackPanel labelled(hstring const& label, FrameworkElement const& control,
                                    double width = 0.0) {
  auto stack = column(2.0);
  stack.Children().Append(caption(label));
  if (width > 0.0) {
    control.Width(width);
  }
  stack.Children().Append(control);
  return stack;
}

inline Controls::Button accent_button(hstring const& label) {
  Controls::Button b;
  b.Content(box_value(label));
  auto res = Application::Current().Resources();
  if (res.HasKey(box_value(L"AccentButtonStyle"))) {
    if (auto style = res.Lookup(box_value(L"AccentButtonStyle")).try_as<Style>()) b.Style(style);
  }
  return b;
}

inline Controls::Button button(hstring const& label) {
  Controls::Button b;
  b.Content(box_value(label));
  return b;
}

inline Controls::TextBox field(hstring const& placeholder, hstring const& value = L"") {
  Controls::TextBox t;
  t.PlaceholderText(placeholder);
  if (!value.empty()) t.Text(value);
  t.MinWidth(120.0);
  return t;
}

// Grow a child to fill its grid column so fields line up regardless of their neighbours' widths.
inline GridLength star() { return GridLength{1.0, GridUnitType::Star}; }
inline GridLength automatic() { return GridLength{0.0, GridUnitType::Auto}; }

inline Controls::RowDefinition rows(GridLength const& height) {
  Controls::RowDefinition d;
  d.Height(height);
  return d;
}

inline Controls::ColumnDefinition cols(GridLength const& width) {
  Controls::ColumnDefinition d;
  d.Width(width);
  return d;
}

inline void stretch(FrameworkElement const& e) {
  e.HorizontalAlignment(HorizontalAlignment::Stretch);
}

}  // namespace SyncPlayer::ui

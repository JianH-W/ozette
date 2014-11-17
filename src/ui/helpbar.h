//
// lindi
// Copyright (C) 2014 Mars J. Saxman
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#ifndef UI_HELPBAR_H
#define UI_HELPBAR_H

#include <string>

namespace UI {
namespace HelpBar {
struct Label {
	Label(char m, bool c, std::string t);
	Label();
	char mnemonic;
	bool is_ctrl;
	std::string text;
};
struct Panel {
        static const size_t kWidth = 6;
        static const size_t kHeight = 2;
        Label label[kHeight][kWidth];
};
} // namespace HelpBar
} // namespace UI

#endif //UI_HELPBAR_H
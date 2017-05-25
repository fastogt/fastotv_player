/*  Copyright (C) 2014-2017 FastoGT. All right reserved.

    This file is part of FastoTV.

    FastoTV is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    FastoTV is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FastoTV. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <vector>

#include "url.h"

#define CHANNELS_FIELD "channels"

namespace fasto {
namespace fastotv {

typedef std::vector<Url> channels_t;

json_object* MakeJobjectFromChannels(const channels_t& channels);  // allocate json_object
channels_t MakeChannelsClass(json_object* obj);                    // pass valid json obj

}  // namespace fastotv
}  // namespace fasto
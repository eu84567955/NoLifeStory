//////////////////////////////////////////////////////////////////////////////
// NoLifeClient - Part of the NoLifeStory project                           //
// Copyright © 2013 Peter Atashian                                          //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////

#include "foothold.hpp"
#include "map.hpp"
#include <algorithm>
#include <string>

namespace nl {
    std::vector<foothold> footholds;
    foothold::foothold(node n, unsigned id, unsigned group, unsigned layer)
        : id(id), group(group), layer(layer) {
        x1 = n["x1"];
        x2 = n["x2"];
        y1 = n["y1"];
        y2 = n["y2"],
        force = n["force"];
        piece = n["piece"];
        nextid = n["next"];
        previd = n["prev"];
        cant_through = n["cantThrough"].get_bool();
        forbid_fall_down = n["forbidFallDown"].get_bool();
        if (nextid < footholds.size())
            next = &footholds[nextid];
        if (previd < footholds.size())
            prev = &footholds[previd];
        initialized = true;
    }
    void foothold::load() {
        footholds.clear();
        auto s = 0u;
        for (auto layern : map::current["foothold"])
        for (auto groupn : layern)
        for (auto idn : groupn) {
            s = std::max(static_cast<unsigned>(std::stoi(idn.name())), s);
        }
        footholds.resize(s + 1);
        for (auto layern : map::current["foothold"]) {
            auto layeri = static_cast<unsigned>(std::stoi(layern.name()));
            for (auto groupn : layern) {
                auto groupi = static_cast<unsigned>(std::stoi(groupn.name()));
                for (auto idn : groupn) {
                    auto idi = static_cast<unsigned>(std::stoi(idn.name()));
                    footholds[idi] = foothold(idn, idi, groupi, layeri);
                }
            }
        }
    }
}

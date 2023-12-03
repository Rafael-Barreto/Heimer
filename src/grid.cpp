// This file is part of Heimer.
// Copyright (C) 2019 Jussi Lind <jussi.lind@iki.fi>
//
// Heimer is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// Heimer is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Heimer. If not, see <http://www.gnu.org/licenses/>.

#include "grid.hpp"

#include <cmath>

Grid::Grid()
{
    m_lines.reserve(1024);
}

const Grid::LineArray & Grid::calculateLines(const QRectF & sceneRect) const
{
    m_lines.clear();

    if (m_size) {
        // Vertical lines
        auto x = snapToGrid({ sceneRect.left(), sceneRect.top() }).x();
        while (x < sceneRect.right()) {
            m_lines.push_back({ x, sceneRect.top(), x, sceneRect.bottom() });
            x += m_size;
        }
        // Horizontal lines
        auto y = snapToGrid({ sceneRect.left(), sceneRect.top() }).y();
        while (y < sceneRect.bottom()) {
            m_lines.push_back({ sceneRect.left(), y, sceneRect.right(), y });
            y += m_size;
        }
    }

    return m_lines;
}

void Grid::setSize(int size)
{
    m_size = size;
}

int Grid::size() const
{
    return m_size;
}

QPointF Grid::snapToGrid(QPointF in) const
{
    if (!m_size) {
        return in;
    }

    return {
        static_cast<double>(std::round(in.x() / m_size) * m_size),
        static_cast<double>(std::round(in.y() / m_size) * m_size)
    };
}

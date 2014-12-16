/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2014 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/
//  Credits:
//  I gratefully acknowledge the inspiring work of Maxim Shemanarev (McSeem),
//  author of Anti-Grain Geometry (http://www.antigrain.com). I have used
//  the datastructure from AGG as a template for my own.

#ifndef MAPNIK_VERTEX_VECTOR_HPP
#define MAPNIK_VERTEX_VECTOR_HPP

// mapnik
#include <mapnik/vertex.hpp>
#include <mapnik/noncopyable.hpp>

// stl
#include <algorithm>
#include <tuple>
#include <cstdint>
#include <vector>

namespace mapnik
{

template <typename T>
class vertex_vector : private mapnik::noncopyable
{
    using coord_type = T;
public:
    // required for iterators support
    using value_type = std::tuple<unsigned,coord_type,coord_type>;
    using size_type = std::size_t;
    using command_size = std::uint8_t;
    using cont_type = std::vector<value_type>;
private:
    cont_type vertices_;

public:

    vertex_vector()
        : vertices_() {}

    ~vertex_vector() {}

    inline void reserve(std::size_t size)
    {
        vertices_.reserve(size);
    }

    size_type size() const
    {
        return vertices_.size();
    }

    inline void resize(std::size_t size)
    {
        vertices_.resize(size);
    }
 
    inline void shrink_to_fit()
    {
        vertices_.shrink_to_fit();
    }

    inline size_type capacity() const
    {
        return vertices_.capacity();
    }

    void push_back(coord_type x, coord_type y, command_size command)
    {
        vertices_.emplace_back(command,x,y);
    }

    unsigned get_vertex(unsigned pos, coord_type* x, coord_type* y) const
    {
        if (pos >= size()) return SEG_END;
        value_type const& v = vertices_[pos];
        *x = std::get<1>(v);
        *y = std::get<2>(v);
        return std::get<0>(v);
    }
};

}

#endif // MAPNIK_VERTEX_VECTOR_HPP

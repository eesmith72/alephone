/*
 ClassicRenderer.h
 
 Copyright (C) 1991-2001 and beyond by Bungie Studios, Inc.
 and the "Aleph One" developers.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 This license is contained in the file "COPYING",
 which is included with this source code; it is available online at
 http://www.gnu.org/licenses/gpl.html
 */

#ifndef ClassicRenderer_h
#define ClassicRenderer_h

#include "cseries.hpp"

#include "Renderer.h"
#include "ClassicRasterizer.h"


class ClassicRenderer : public Renderer
{
public:
    
    ClassicRenderer() : Renderer(&classic_rasterizer) {}
    ~ClassicRenderer() = default;
    
    void render_tree() override
    {
        Renderer::render_tree();
        if (view->weapons_in_hand_is_visible) { render_weapons_in_hand(kDiffuse); } // render_step arg is ignored in Classic
    }
        
private:
    
    ClassicRasterizer classic_rasterizer;
    
    void render_weapon_in_hand(billboard_t& billboard, RenderStep render_step) override;
};


#endif /* ClassicRenderer_h */

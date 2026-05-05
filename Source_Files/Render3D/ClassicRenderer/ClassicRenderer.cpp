/*
 ClassicRenderer.cpp
 
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

#include "ClassicRenderer.h"

#include "lightsource.h"


void ClassicRenderer::render_viewer_sprite_layer()
{
    rectangle_definition textured_rectangle;
    weapon_display_information display_data;
    shape_information_data *shape_information;
    
    // Need to set this...
    RasPtr->SetForeground();
    
    // No models here, and completely opaque
    textured_rectangle.ModelPtr = NULL;
    textured_rectangle.Opacity = 1;

    /* get_weapon_display_information() returns true if there is a weapon to be drawn.  it
        should initially be passed a count of zero.  it returns the weapon’s texture and
        enough information to draw it correctly. */
    short count= 0;
    while (get_weapon_display_information(&count, &display_data))
    {
        /* fetch relevant shape data */
        shape_information = extended_get_shape_information(display_data.collection, display_data.low_level_shape_index);
        // Nonexistent frame: skip
        if (!shape_information) continue;
        
        // No need for a fake sprite rectangle, since models are foreground objects
        
        textured_rectangle.ShapeDesc = BUILD_DESCRIPTOR(display_data.collection,0);
        textured_rectangle.LowLevelShape = display_data.low_level_shape_index;
        
        if (shape_information->flags&_X_MIRRORED_BIT) display_data.flip_horizontal= !display_data.flip_horizontal;
        if (shape_information->flags&_Y_MIRRORED_BIT) display_data.flip_vertical= !display_data.flip_vertical;

        /* calculate shape rectangle */
        position_sprite_axis(&textured_rectangle.x0, &textured_rectangle.x1, view->screen_height, view->screen_width, display_data.horizontal_positioning_mode,
            display_data.horizontal_position, display_data.flip_horizontal, shape_information->world_left, shape_information->world_right);
        position_sprite_axis(&textured_rectangle.y0, &textured_rectangle.y1, view->screen_height, view->screen_height, display_data.vertical_positioning_mode,
            display_data.vertical_position, display_data.flip_vertical, -shape_information->world_top, -shape_information->world_bottom);
        
        /* set rectangle bitmap and shading table */
        extended_get_shape_bitmap_and_shading_table(display_data.collection, display_data.low_level_shape_index, &textured_rectangle.texture, &textured_rectangle.shading_tables, view->shading_mode);
        if (!textured_rectangle.texture) continue;
        
        textured_rectangle.flags= 0;

        /* initialize clipping window to full screen */
        textured_rectangle.clip_left= 0;
        textured_rectangle.clip_right= view->screen_width;
        textured_rectangle.clip_top= 0;
        textured_rectangle.clip_bottom= view->screen_height;

        /* copy mirror flags */
        textured_rectangle.flip_horizontal= display_data.flip_horizontal;
        textured_rectangle.flip_vertical= display_data.flip_vertical;
        
        /* lighting: depth of zero in the camera’s polygon index */
        textured_rectangle.depth= 0;
        textured_rectangle.ambient_shade= get_light_intensity(get_polygon_data(view->origin_polygon_index)->floor_lightsource_index);
        textured_rectangle.ambient_shade= MAX(shape_information->minimum_light_intensity, textured_rectangle.ambient_shade);
        if (view->shading_mode==_shading_infravision) textured_rectangle.flags|= _SHADELESS_BIT;

        // Calculate the object's horizontal position
        // for the convenience of doing teleport-in/teleport-out
        textured_rectangle.xc = (textured_rectangle.x0 + textured_rectangle.x1) >> 1;
        
        /* make the weapon reflect the owner’s transfer mode */
        instantiate_rectangle_transfer_mode(view, &textured_rectangle, display_data.transfer_mode, display_data.transfer_phase);
        /* and draw it */
        RasPtr->texture_rectangle(textured_rectangle);
    }
}

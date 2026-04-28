/*
 interpolated_world.h -- Storage for interpolated (> 30 fps) world
 
 Copyright (C) 2021 Gregory Smith and the "Aleph One" developers.
 
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

#ifndef INTERPOLATED_WORLD_H
#define INTERPOLATED_WORLD_H


#include "cseries.h"
#include "world.h" // angle, fixed_angle, world_point3d

struct TickWorldView
{
    int16_t origin_polygon_index;
    angle yaw, pitch;
    fixed_angle virtual_yaw, virtual_pitch;
    world_point3d origin;
    _fixed maximum_depth_intensity;
};

struct weapon_display_information;

void init_interpolated_world();
void enter_interpolated_world();
void exit_interpolated_world();

void update_main_camera(int32_t ticks_elapsed);

void track_contrail_interpolation(int16_t projectile_index, int16_t effect_index);
bool get_interpolated_weapon_display_information(short* count, weapon_display_information* data);

#endif

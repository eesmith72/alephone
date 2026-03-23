/*
 *  QuickSave.cpp - a manager for auto-named saved games
 
	Copyright (C) 2014 and beyond by Jeremiah Morris
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

#include "cseries.h"
#include "QuickSave.h"

#include "choose_file_dialogs_os.hpp"

#include "DataFile.hpp"
#include "world.h"
#include "map.h"
#include "wad.h"
#include "overhead_map.h"
#include "screen_drawing.h"
#include "Canvas.hpp"
#include "interface.h"
#include "preferences.h"
#include "shell.h"
#include "player.h"
#include "map_wad.h"
#include "sdl_dialogs.h"
#include "sdl_widgets.h"
#include "images.h"
#include "sdl_resize.h"
#include "SDL_rwops_ostream.h"
#include "WadImageCache.h"
#include "InfoTree.h"

namespace algo = boost::algorithm;

const int RENDER_WIDTH = 1280;
const int RENDER_HEIGHT = 720;
const int RENDER_SCALE = OVERHEAD_MAP_MAXIMUM_SCALE;

const int PREVIEW_WIDTH = 128;
const int PREVIEW_HEIGHT = 72;

ao_err create_updated_save(QuickSave& save);



class QuickSaveImageCache
{
public:
    typedef std::pair<std::string, SDL_Surface*> cache_pair_t;
    typedef std::list<cache_pair_t>::iterator cache_iter_t;
    
    static QuickSaveImageCache* instance();
    
    SDL_Surface* get(std::string image_name);
    void clear();

private:
    QuickSaveImageCache() {};
    static const int k_max_items = 100;
    
    std::list<cache_pair_t> m_used;
    std::map<std::string, cache_iter_t> m_images;
};

QuickSaveImageCache* QuickSaveImageCache::instance()
{
    static QuickSaveImageCache* m_instance = nullptr;
    if (!m_instance) {
        m_instance = new QuickSaveImageCache;
    }
    
    return m_instance;
}


SDL_Surface* QuickSaveImageCache::get(std::string image_name)
{
    const auto& it = m_images.find(image_name);
    if (it != m_images.end())
    {
        // found it: move to front of list
        m_used.splice(m_used.begin(), m_used, it->second);
        return it->second->second;
    }
    
    // didn't find: load image
	WadImageDescriptor desc;
	desc.file_path = get_quicksaves_dir() / (image_name + ".sgaA");
	desc.checksum = 0;
	desc.index = SAVE_GAME_METADATA_INDEX;
	desc.tag = SAVE_IMG_TAG;
	
	SDL_Surface *img = WadImageCache::instance()->get_image(desc, PREVIEW_WIDTH, PREVIEW_HEIGHT);
	if (img) {
        m_used.push_front(cache_pair_t(image_name, img));
        m_images[image_name] = m_used.begin();
        
        // enforce maximum cache size
        if (m_used.size() > k_max_items) {
            cache_iter_t lru = m_used.end();
            --lru;
            m_images.erase(lru->first);
            SDL_FreeSurface(lru->second);
            m_used.pop_back();
        }
    }
    return img;
}


void QuickSaveImageCache::clear()
{
    m_images.clear();
    for (const auto& it : m_used) { SDL_FreeSurface(it.second); }
    m_used.clear();
}


class w_saves : public w_list_base
{
public:
    w_saves(std::vector<QuickSave>& saves, int width, int numRows) : w_list_base(width, numRows), m_saves(saves)
    {
        saved_min_height = item_height() * static_cast<uint16>(shown_items) + get_theme_space(LIST_WIDGET, T_SPACE) + get_theme_space(LIST_WIDGET, B_SPACE);
        trough_rect.h = saved_min_height - get_theme_space(LIST_WIDGET, TROUGH_T_SPACE) - get_theme_space(LIST_WIDGET, TROUGH_B_SPACE);
        new_items();
    }
    
    void mouse_move(int x, int y);
    void click(int x, int y);
    uint16 item_height() { return PREVIEW_HEIGHT + 6; }
    QuickSave selected_save() { return m_saves[get_selection()]; }
    void remove_selected();
    void update_selected(QuickSave& save) { m_saves[get_selection()] = save; dirty = true; }
    bool has_selection() { return m_saves.size() > 0; }
    
    int32_t count() const { return (int32_t)m_saves.size(); }

protected:
    void draw_items(Canvas* canvas);
    void item_selected();
    
private:
    std::vector<QuickSave>& m_saves;
    void draw_item(QuickSaves::iterator it, Canvas* canvas, int16 x, int16 y, uint16 width, bool selected);
};

void w_saves::remove_selected()
{
    m_saves.erase(m_saves.begin()+get_selection());
    new_items();
}

void w_saves::mouse_move(int x, int y)
{
    if (thumb_dragging) {
        w_list_base::mouse_move(x, y);
    }
}

void w_saves::click(int x, int y)
{
    if (x == 0 && y == 0 && active) {
        // almost certainly a simulated click
        if (count() > 0)
            item_selected();
    }
    else if (x >= trough_rect.x && x < trough_rect.x + trough_rect.w
             && y >= thumb_y && y <= thumb_y + thumb_height) {
        thumb_dragging = dirty = true;
        thumb_drag_y = y - thumb_y;
    } else {
        if (x < get_theme_space(LIST_WIDGET, L_SPACE) || x >= rect.w - get_theme_space(LIST_WIDGET, R_SPACE)
            || y < get_theme_space(LIST_WIDGET, T_SPACE) || y >= rect.h - get_theme_space(LIST_WIDGET, B_SPACE))
            return;
        
        if ((y - get_theme_space(LIST_WIDGET, T_SPACE)) / item_height() + top_item < std::min(count(), top_item + shown_items))
        {
            size_t old_sel = selection;
            set_selection((y - get_theme_space(LIST_WIDGET, T_SPACE)) / item_height() + top_item);
            if (selection == old_sel && count() > 0 && is_item_selectable(selection))
                item_selected();
        }
    }
}

void w_saves::draw_items(Canvas* canvas)
{
    QuickSaves::iterator i = m_saves.begin();
    int16 x = rect.x + get_theme_space(LIST_WIDGET, L_SPACE);
    int16 y = rect.y + get_theme_space(LIST_WIDGET, T_SPACE);
    uint16 width = rect.w - get_theme_space(LIST_WIDGET, L_SPACE) - get_theme_space(LIST_WIDGET, R_SPACE);
    
    for (size_t n = 0; n < top_item; ++n)
    {
        ++i;
    }
    
    for (size_t n = top_item; n < top_item + MIN(shown_items, count()); ++n, ++i, y = y + item_height())
        draw_item(i, canvas, x, y, width, n == selection);
}

void w_saves::item_selected()
{    
    get_owning_dialog()->quit(0);
}

void w_saves::draw_item(QuickSaves::iterator it, Canvas* canvas, int16 x, int16 y, uint16 width, bool selected) 
{
    std::ostringstream oss;
    oss << it->save_time;
    SDL_Surface *image = QuickSaveImageCache::instance()->get(oss.str());
    SDL_Rect r = {x + 3, y + 3, PREVIEW_WIDTH, PREVIEW_HEIGHT};
    canvas->draw_surface(image, {x + 3, y + 3});
    //SDL_BlitSurface(image, NULL, canvas, &r);
    x += PREVIEW_WIDTH + 12;
    width -= PREVIEW_WIDTH + 12;
    
    SDL_Color color = get_theme_color(ITEM_WIDGET, selected ? ACTIVE_STATE : DEFAULT_STATE);
    
    canvas->set_clip({x, 0, width, canvas->h});
    
    // TODO: FIX
    
    y += get_font()->ascent;
    if (it->name.length())
    {
        canvas->draw_text(it->name, get_font(), color, {x, y});
        y += get_font()->ascent + 1;
    }
    canvas->draw_text(it->formatted_time, get_font(), color, {x, y});
    
    y += get_font()->ascent + 1;
    canvas->draw_text(it->level_name, get_font(), color, {x, y});
    
    y += get_font()->ascent + 1;
    
    std::string game_time = it->formatted_ticks;
    if (it->players > 1) { game_time += " (Cooperative Play)"; }
    
    canvas->draw_text(game_time, get_font(), color, {x, y});
    
    canvas->clear_clip();
}

// Allow rename dialog to be closed by hitting Return in the text field
class w_save_name : public w_text_entry
{
public:
    w_save_name(dialog *d, const std::string& initial_name = NULL) : w_text_entry(256, initial_name), parent(d) {}
    ~w_save_name() {}
    
    void event(SDL_Event & e)
    {
        // Return = close dialog
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN)
            parent->quit(0);
        w_text_entry::event(e);
    }
    
private:
    dialog *parent;
};


const int LOAD_DIALOG_OTHER = 4;
static void dialog_exit_other(void *arg)
{
    dialog *d = static_cast<dialog *>(arg);
    d->quit(LOAD_DIALOG_OTHER);
}

const int iDIALOG_SAVES_W = 42;
const int iDIALOG_RENAME_W = 43;
const int iDIALOG_DELETE_W = 44;
const int iDIALOG_EXPORT_W = 45;
const int iDIALOG_ACCEPT_W = 46;

static void dialog_rename(void *arg)
{
    dialog *d = static_cast<dialog *>(arg);
    w_saves *saves_w = static_cast<w_saves *>(d->get_widget_by_id(iDIALOG_SAVES_W));
    QuickSave sel = saves_w->selected_save();

    dialog rd;
    vertical_placer *placer = new vertical_placer;
    
    horizontal_placer* name_placer = new horizontal_placer;
    w_text_entry *rename_w = new w_save_name(&rd, sel.name);
    name_placer->dual_add(rename_w->adding_label("Name: "), rd);
    name_placer->dual_add(rename_w, rd);
    placer->add(name_placer, true);
    
    placer->add(new w_spacer(), true);

    horizontal_placer* button_placer = new horizontal_placer;
    w_button* accept_w = new w_button("RENAME", dialog_ok, &rd);
    button_placer->dual_add(accept_w, rd);
    w_button* cancel_w = new w_button("CANCEL", dialog_cancel, &rd);
    button_placer->dual_add(cancel_w, rd);
    placer->add(button_placer, true);
    
    rd.set_widget_placer(placer);
    rd.activate_widget(rename_w);
    if (rd.run() == 0) {
        sel.name = rename_w->get_text();
		create_updated_save(sel);
        saves_w->update_selected(sel);
    }
}
static void dialog_delete(void *arg)
{
    dialog *d = static_cast<dialog *>(arg);
    w_saves *saves_w = static_cast<w_saves *>(d->get_widget_by_id(iDIALOG_SAVES_W));
    QuickSave sel = saves_w->selected_save();
	
	dialog rd;
	vertical_placer *placer = new vertical_placer;
	placer->add(new w_spacer, true);
	placer->dual_add(new w_static_text("Delete this save?"), rd);
	placer->add(new w_spacer, true);

	std::vector<QuickSave> saves;
	saves.push_back(sel);
	w_saves* selsave_w = new w_saves(saves, 400, 1);
	placer->dual_add(selsave_w, rd);
	placer->add(new w_spacer, true);
	
	horizontal_placer* button_placer = new horizontal_placer;
	w_button* accept_w = new w_button("DELETE", dialog_ok, &rd);
	button_placer->dual_add(accept_w, rd);
	w_button* cancel_w = new w_button("CANCEL", dialog_cancel, &rd);
	button_placer->dual_add(cancel_w, rd);
	placer->add(button_placer, true);
	rd.set_widget_placer(placer);
	rd.activate_widget(accept_w);

    if (rd.run() == 0 && delete_quick_save(sel)) {
        saves_w->remove_selected();
        if (!saves_w->has_selection()) {
			w_tiny_button* rename_w = static_cast<w_tiny_button *>(d->get_widget_by_id(iDIALOG_RENAME_W));
            if (rename_w) rename_w->set_enabled(false);
            w_tiny_button* delete_w = static_cast<w_tiny_button *>(d->get_widget_by_id(iDIALOG_DELETE_W));
            if (delete_w) delete_w->set_enabled(false);
            w_tiny_button* export_w = static_cast<w_tiny_button *>(d->get_widget_by_id(iDIALOG_EXPORT_W));
            if (export_w) export_w->set_enabled(false);
            w_button* accept_w = static_cast<w_button *>(d->get_widget_by_id(iDIALOG_ACCEPT_W));
            if (accept_w) accept_w->set_enabled(false);
        }
    }
}


static void dialog_export(void *arg)
{
    dialog *d = static_cast<dialog *>(arg);
    w_saves *saves_w = static_cast<w_saves *>(d->get_widget_by_id(iDIALOG_SAVES_W));
    QuickSave sel = saves_w->selected_save();
    
    ao_path start = get_saved_games_dir() / (sel.name.empty() ? sel.level_name : sel.name);
    ao_path dst_path = display_export_saved_game_dialog();
    if (!dst_path.empty())
    {
        std::error_code code;
        std::filesystem::rename(sel.save_file, dst_path, code);
        if (code)
        {
            notify_user(STRID(strERRORS, fileError), "Filesystem error $code$: $text$", {
                {"$code$", [code]{ return std::to_string(code.value()); }},
                {"$text$", [code]{ return code.message(); }},
            });
        }
    }
}




ao_path display_load_saved_game_dialog()
{
    QuickSaves::instance()->enumerate();

    dialog d;
    vertical_placer *placer = new vertical_placer;
    w_title *w_header = new w_title("CONTINUE SAVED GAME");
    placer->dual_add(w_header, d);
    placer->add(new w_spacer, true);
    
    horizontal_placer *mini_button_placer = new horizontal_placer;
    w_tiny_button *rename_w = new w_tiny_button("RENAME", dialog_rename, &d);
    rename_w->set_identifier(iDIALOG_RENAME_W);
    mini_button_placer->dual_add(rename_w, d);
#ifndef MAC_APP_STORE
    w_tiny_button *export_w = new w_tiny_button("EXPORT", dialog_export, &d);
    export_w->set_identifier(iDIALOG_EXPORT_W);
    mini_button_placer->dual_add(export_w, d);
#endif
    w_tiny_button *delete_w = new w_tiny_button("DELETE", dialog_delete, &d);
    delete_w->set_identifier(iDIALOG_DELETE_W);
    mini_button_placer->dual_add(delete_w, d);
    
    placer->add(mini_button_placer, true);
    placer->add(new w_spacer, true);

    std::vector<QuickSave> saves(QuickSaves::instance()->begin(), QuickSaves::instance()->end());
    w_saves* saves_w = new w_saves(saves, 400, 4);
    saves_w->set_identifier(iDIALOG_SAVES_W);
    placer->dual_add(saves_w, d);
    placer->add(new w_spacer, true);

    horizontal_placer* button_placer = new horizontal_placer;
#ifndef MAC_APP_STORE
    w_button* other_w = new w_button("LOAD OTHER", dialog_exit_other, &d);
    button_placer->dual_add(other_w, d);
#endif
    w_button* accept_w = new w_button("LOAD", dialog_ok, &d);
    accept_w->set_identifier(iDIALOG_ACCEPT_W);
    button_placer->dual_add(accept_w, d);
    w_button* cancel_w = new w_button("CANCEL", dialog_cancel, &d);
    button_placer->dual_add(cancel_w, d);
    
    placer->add(button_placer, true);
    
    d.set_widget_placer(placer);
    d.activate_widget(saves_w);
    
    if (!saves_w->has_selection())
    {
        rename_w->set_enabled(false);
        delete_w->set_enabled(false);
#ifndef MAC_APP_STORE
        export_w->set_enabled(false);
#endif
        accept_w->set_enabled(false);
    }
    
    ao_path result;
    QuickSave sel;
    switch (d.run())
    {
        case 0:
            sel = saves_w->selected_save();
            result = sel.save_file;
            break;
            
        case LOAD_DIALOG_OTHER:
            result = display_read_saved_game_dialog(); // TODO: pass existing file (if any) as starting point
            break;
            
        default: // TODO: what else? Cancel, presumably
            break;
    }
    
    QuickSaves::instance()->clear();
    QuickSaveImageCache::instance()->clear();
    return result;
}

static bool build_map_preview(std::ostringstream& ostream)
{
    SDL_Rect r = {0, 0, RENDER_WIDTH, RENDER_HEIGHT};
    SDL_Surface *surface = SDL_CreateRGBSurface(SDL_SWSURFACE, r.w, r.h, 32, 0xff0000, 0x00ff00, 0x0000ff, 0);
    if (!surface)
        return false;
	
    SDL_FillRect(surface, &r, SDL_MapRGB(surface->format, 0, 0, 0));
	
    struct overhead_map_data overhead_data;
    overhead_data.half_width = r.w >> 1;
    overhead_data.half_height = r.h >> 1;
    overhead_data.width = r.w;
    overhead_data.height = r.h;
    overhead_data.top = overhead_data.left = 0;
    overhead_data.scale = RENDER_SCALE;
    overhead_data.mode = _rendering_saved_game_preview;
    overhead_data.origin.x = local_player->location.x;
    overhead_data.origin.y = local_player->location.y;
	
    bool old_OGL_MapActive = OGL_MapActive;
    _set_port_to_custom(surface);
    OGL_MapActive = false;
    _render_overhead_map(&overhead_data); // TODO: render map using Canvas_SDL, giving us a Surface
    OGL_MapActive = old_OGL_MapActive;
    _restore_port();
	
    SDL_RWops *rwops = SDL_RWFromOStream(ostream);
#if defined (HAVE_SDL_IMAGE) && defined (HAVE_PNG)
	int ret = IMG_SavePNG_RW(surface, rwops, 0);
#else
    int ret = SDL_SaveBMP_RW(surface, rwops, false);
#endif
    SDL_FreeSurface(surface);
    SDL_RWclose(rwops);
	
    return (ret == 0);
}

std::string build_save_metadata(QuickSave& save)
{
	InfoTree pt;
	pt.put("name", save.name);
	pt.put("level_name", save.level_name);
	pt.put("ticks", save.ticks);
	pt.put("ticks_formatted", save.formatted_ticks);
	pt.put("time", save.save_time);
	pt.put("time_formatted", save.formatted_time);
	pt.put("players", save.players);
	
	std::ostringstream xout;
	pt.save_ini(xout);
	
	return xout.str();
}


ao_err create_updated_save(QuickSave& save) // EES: this is bizarre: it's called in Open dialog when user clicks RENAME button, but it's doing an awful lot of work, reading the saved-game file's WAD data and metadata, then writing it to a temp file and renaming that... so why not just rename the original saved-game file (see also save_game_to_file)
{
    ao_err err = no_err;
    
	// read data from existing save file
	int32 game_wad_length = 0;
	std::string imagedata;
	
	DataFile currentFile;
    err = currentFile.open(save.save_file);
    if (err) return err;
    
    wad_header_t header;
    err = read_wad_header(currentFile, &header);
    if (err) return err;
    
    wad_data* map_wad;
    err = read_indexed_wad_from_file(currentFile, &header, 0, false, map_wad);
    if (err) return err;
    
    if (map_wad) game_wad_length = calculate_wad_length(&header, map_wad);

    wad_data* orig_meta_wad = nullptr;
    err = read_indexed_wad_from_file(currentFile, &header, SAVE_GAME_METADATA_INDEX, true, orig_meta_wad);
    if (!err) // think this one is allowed to fail
    {
        size_t data_length;
        char *raw_imagedata = (char*)get_wad_resource_for_tag(orig_meta_wad, SAVE_IMG_TAG, &data_length);
        imagedata = std::string(raw_imagedata, data_length);
    }
    
    currentFile.close();
	
	// create updated save file
    ao_path temp_path = save.save_file;
    err = make_temp_file(temp_path);
    if (err) return err;
    
    DataFile SaveFile;
    err = SaveFile.open(temp_path, DataFile::mode_binary_write);
    if (err) return err;

    write_wad_header(SaveFile, &header);
    
    int32_t offset = SIZEOF_wad_header;
    directory_entry entries[2];
    set_indexed_directory_offset_and_length(&header, entries, 0, offset, game_wad_length, 0);
    
    write_wad(SaveFile, &header, map_wad, offset);
    
    offset += game_wad_length;
    header.directory_offset= offset;
    int32_t meta_wad_length;
    wad_data* new_meta_wad = build_meta_game_wad(build_save_metadata(save), imagedata, &header, &meta_wad_length);
    
    set_indexed_directory_offset_and_length(&header, entries, 1, offset, meta_wad_length, SAVE_GAME_METADATA_INDEX);
    
    write_wad(SaveFile, &header, new_meta_wad, offset);
    offset += meta_wad_length;
    header.directory_offset = offset;
    
    write_wad_header(SaveFile, &header);
    write_directorys(SaveFile, &header, entries);
    
    free_wad(new_meta_wad);
    free_wad(map_wad);
    free_wad(orig_meta_wad);
    
    // rename the temp file
    std::error_code code;
    std::filesystem::rename(temp_path, save.save_file, code);
    if (code) { err = code.value(); } // TODO: what error?
	
    return err;
}


bool create_quick_save(void)
{
    QuickSave save;
    
    time(&(save.save_time));
    char fmt_time[256];
    tm *time_info = localtime(&(save.save_time));
    strftime(fmt_time, 256, "%x %H:%M", time_info);
    save.formatted_time = fmt_time;

    save.level_name = static_world->level_name;
    save.players = dynamic_world->player_count;
    save.ticks = dynamic_world->tick_count;
    
    char fmt_ticks[256];
    if (save.ticks < 60*TICKS_PER_MINUTE)
    {
        snprintf(fmt_ticks, sizeof(fmt_ticks), "%d:%02d",
                 save.ticks/TICKS_PER_MINUTE, (save.ticks/TICKS_PER_SECOND) % 60);
    }
    else
    {
        snprintf(fmt_ticks, sizeof(fmt_ticks), "%d:%02d:%02d",
                 save.ticks/(60*TICKS_PER_MINUTE), (save.ticks/TICKS_PER_MINUTE) % 60, (save.ticks/TICKS_PER_SECOND) % 60);
    }
    save.formatted_ticks = fmt_ticks;
    
    save.save_file = get_quicksaves_dir() / (std::to_string(save.save_time) + ".sgaA"); // TODO: why not datestamp?
	
    std::string metadata = build_save_metadata(save);
    std::ostringstream image_stream;
    bool success = build_map_preview(image_stream); // don't think we care if this fails
    
    ao_err err = save_game_to_file(save.save_file, metadata, image_stream.str());
    
    if (!err) { QuickSaves::instance()->delete_surplus_saves(environment_preferences.maximum_quick_saves); }
    return err;
}

bool delete_quick_save(QuickSave& save)
{
	// delete cached images
	WadImageDescriptor desc;
	desc.file_path = save.save_file;
	desc.checksum = 0;
	desc.index = SAVE_GAME_METADATA_INDEX;
	desc.tag = SAVE_IMG_TAG;
	WadImageCache::instance()->remove_image(desc);
    std::error_code code;
    std::filesystem::remove(save.save_file, code); // TODO: suspect we'll end up consolidating std::filesystem operations in a file_utilities.cpp, where we can map OS-specific FS error to standard AO errors, but not going to bother right now
	return !code;
}



void ParseQuickSave(const ao_path& file_name)
{
	DataFile file;
    ao_err err = file.open(file_name);
    if (err) return;
    
    wad_header_t header;
    err = read_wad_header(file, &header);
    if (err) return;
    
    wad_data* wad;
    err = read_indexed_wad_from_file(file, &header, SAVE_GAME_METADATA_INDEX, true, wad);
    if (err) return;
    
    size_t data_length;
    char *raw_metadata = (char *)get_wad_resource_for_tag(wad, SAVE_META_TAG, &data_length);
    std::string metadata = std::string(raw_metadata, data_length);
    
    InfoTree pt;
    std::istringstream strm(metadata);
    try
    {
        pt = InfoTree::load_ini(strm);
    }
    catch (const InfoTree::Exception& e)
    {
        free_wad(wad);
        return;
    }
    
    QuickSave Data = QuickSave();
    Data.save_file = file_name;
    pt.read("name", Data.name);
    pt.read("level_name", Data.level_name);
    pt.read("ticks", Data.ticks);
    pt.read("ticks_formatted", Data.formatted_ticks);
    pt.read("time", Data.save_time);
    pt.read("time_formatted", Data.formatted_time);
    pt.read("players", Data.players);
    QuickSaves::instance()->add(Data);
    
    free_wad(wad);
}


void ParseDirectory(const ao_path& dir)
{
    if (!std::filesystem::is_directory(dir))
    {
        log_warning_f("No directory found at: '%s'", dir.c_str());
        return;
    }

    for (const ao_path& path : std::filesystem::directory_iterator(dir))
    {
        if (path.extension() == ".sgaA") { ParseQuickSave(path); }
    }
}




QuickSaves* QuickSaves::instance() {
	static QuickSaves* m_instance = nullptr;
    if (!m_instance) {
        m_instance = new QuickSaves;
    }
    
    return m_instance;
}

void QuickSaves::enumerate() {
    clear();
	
    log_context("parsing quick saves");
    
    ao_path path = get_quicksaves_dir();
    ParseDirectory(path);
    std::sort(m_saves.begin(), m_saves.end());
    std::reverse(m_saves.begin(), m_saves.end());
}

void QuickSaves::clear() {
    m_saves.clear();
}


// TODO: FIX: ignoring pruning for now

/*
bool most_recent_dir_entry(const dir_entry& a, const dir_entry& b)
{
    return a.date > b.date;
}
 */

void QuickSaves::delete_surplus_saves(size_t max_saves)
{
    /*
    if (max_saves < 1)
        return;     // unlimited saves, no need to prune
    clear();
    
    // Check the directory to count the saves. If there
    // are fewer than the max, no need to go further.
    std::vector<dir_entry> entries;
    ao_path path = get_quicksaves_dir();
    
    path.SetToQuickSavesDir();
    if (path.ReadDirectory(entries)) {
        if (entries.size() <= max_saves)
            return;
    }
    
    // We might have too many unnamed saves; load and
    // count them, deleting any extras.
    enumerate();
    size_t unnamed_saves = 0;
    for (std::vector<QuickSave>::iterator it = begin(); it != end(); ++it) {
        if (it->name.length())
            continue;
        if (++unnamed_saves > max_saves)
            delete_quick_save(*it);
    }
    clear();
     */
}


/* -*- C++ -*-
 *
 *  PonscripterLabel_text.cpp - Text parser of Ponscripter
 *
 *  Copyright (c) 2001-2007 Ogapee (original ONScripter, of which this
 *  is a fork).
 *
 *  ogapee@aqua.dti2.ne.jp
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA
 */

#include "PonscripterLabel.h"

/* Text-plane sink (see src/engine_bridge.h). Included only in the libretro
 * build: a standalone engine has no shell to publish to, so publishTextPlane
 * compiles to nothing there. */
#ifdef __LIBRETRO__
#include "engine_bridge.h"
#endif

Glyph
PonscripterLabel::renderGlyph(Font* font, Uint16 text, int size,
                              float x_fractional_part)
{
    font->set_size(size);
     /* Initializing SDL_Color.unused here to silence warnings about unused
       variables. 32 bit operations should be faster than 24 bit ones anyway.
       Users will be delighted by this 0.0000001 microsecond increase in speed.
     (contribution by Andrius, March 2010) */
    static SDL_Color fcol={0xff, 0xff, 0xff, 0xff}, bcol={0, 0, 0, 0};
    current_glyph = font->render_glyph(text, fcol, bcol, x_fractional_part);
    return current_glyph;
}


void
PonscripterLabel::drawGlyph(SDL_Surface* dst_surface, Fontinfo* info,
        SDL_Color &color, unsigned short unicode, float x, int y,
	bool shadow_flag, AnimationInfo* cache_info, SDL_Rect* clip,
	SDL_Rect &dst_rect)
{
    float minx, maxy;

    int sz = info->doSize();

    info->font()->get_metrics(unicode, &minx, NULL, NULL, &maxy);

    Glyph g = renderGlyph(info->font(), unicode, sz,
			  x + minx - floor(x + minx));
    bool rotate_flag = false;

    if (g.bitmap) {
	minx = g.left;
	maxy = g.top;
    }
    
    dst_rect.x = int(floor(x + minx));
    dst_rect.y = y + info->font()->ascent() - int(ceil(maxy));

    if (shadow_flag) {
        if (info->getRTL())
            dst_rect.x -= shade_distance[0];
        else
            dst_rect.x += shade_distance[0];
        dst_rect.y += shade_distance[1];
    }

    if (g.bitmap) {
        dst_rect.w = g.bitmap->w;
        dst_rect.h = g.bitmap->h;

        if (cache_info == &text_info) {
            // When rendering text
            cache_info->blendText(g.bitmap, dst_rect.x, dst_rect.y,
                                  color, clip);
            cache_info->blendOnSurface(dst_surface, 0, 0, dst_rect);
        }
        else {
            if (cache_info)
                cache_info->blendText(g.bitmap, dst_rect.x, dst_rect.y,
                                      color, clip);

            if (dst_surface)
                alphaBlendText(dst_surface, dst_rect, g.bitmap, color, clip,
                               rotate_flag);
        }
    }
}


// Returns character bytes.
// This is where we process ligatures for display text!
int
PonscripterLabel::drawChar(const char* text, Fontinfo* info, bool flush_flag,
        bool lookback_flag, SDL_Surface* surface, AnimationInfo* cache_info,
	SDL_Rect* clip)
{
    int bytes;
    wchar unicode = file_encoding->DecodeWithLigatures(text, *info, bytes);

    bool code = info->processCode(text);
    if (!code) {
        // info->doSize() called in GlyphAdvance
        wchar next = file_encoding->DecodeWithLigatures(text + bytes, *info);
        float adv = info->GlyphAdvance(unicode, next);
        if (isNonspacing(unicode)) info->advanceBy(-adv);

        if (info->isNoRoomFor(adv)) info->newLine();

        float x = info->GetX() * screen_ratio1 / screen_ratio2;
        if (info->getRTL())
            x -= adv;
        int   y = info->GetY() * screen_ratio1 / screen_ratio2;

        SDL_Color color;
        SDL_Rect  dst_rect = { 0, 0, 0, 0 };
        if (info->is_shadow) {
            color.r = color.g = color.b = 0;
            drawGlyph(surface, info, color, unicode, x, y, true, cache_info,
		      clip, dst_rect);
        }

        color.r = info->color.r;
        color.g = info->color.g;
        color.b = info->color.b;	
        drawGlyph(surface, info, color, unicode, x, y, false, cache_info,
		  clip, dst_rect);

        /* Text-plane capture: this glyph (with its shadow) is what just changed
         * on text_info's surface. `dst_rect` is the glyph's own draw rect from
         * drawGlyph; a copy is widened by addShadeArea to cover the shadow too,
         * the bound the frame's own dirty rect uses — exact for the glyph, with
         * at most the shadow offset as slack. Suppressed while restoreTextBuffer
         * replays a page. The guard skips drawGlyph's no-bitmap early return,
         * which leaves dst_rect unwritten. */
        if (cache_info == &text_info && !text_plane_suppress &&
            dst_rect.w > 0 && dst_rect.h > 0) {
            SDL_Rect plane_rect = dst_rect;
            info->addShadeArea(plane_rect, shade_distance);
            publishTextPlane(plane_rect, false);
        }

	info->addShadeArea(dst_rect, shade_distance);
        if (surface == accumulation_surface && !flush_flag
            && (!clip || AnimationInfo::doClipping(&dst_rect, clip) == 0)) {
            dirty_rect.add(dst_rect);
        }
        else if (flush_flag) {
	    if (surface == accumulation_surface)
		flush(refreshMode()); // hack to fix skip refresh bug
            flushDirect(dst_rect, REFRESH_NONE_MODE);
        }

        /* ---------------------------------------- */
        /* Update text buffer */
        info->advanceBy(adv);
    }

    if (lookback_flag) {
	current_text_buffer->addBytes(text, bytes);
        if (text[0] == '~') current_text_buffer->addBytes(text, bytes);
//TextBuffer_dumpstate(1);
    }
    return bytes;
}


void
PonscripterLabel::drawString(const char* str, rgb_t color, Fontinfo* info,
                             bool flush_flag, SDL_Surface* surface,
                             SDL_Rect* rect, AnimationInfo* cache_info,
                             bool skip_whitespace_flag)
{
    float start_x = info->GetXOffset();
    int   start_y = info->GetYOffset();

/**/float max_x = start_x;
/**/int   max_y = start_y;
    
    /* ---------------------------------------- */
    /* Draw selected characters */
    rgb_t org_color = info->color;
    info->color = color;

    while (*str) {
        while (*str == ' ' && skip_whitespace_flag) str++;

        if (*str == file_encoding->TextMarker()) {
            str++;
            skip_whitespace_flag = false;
            continue;
        }

        if (*str == 0x0a || (*str == '\\' && info->is_newline_accepted)) {
            info->newLine();
            str++;
        }
        else {
            str += drawChar(str, info, false, false, surface, cache_info);
/**/	    if (info->GetXOffset() > max_x) max_x = info->GetXOffset();
/**/	    if (info->GetYOffset() + info->line_space() > max_y)
/**/		max_y = info->GetYOffset() + info->line_space();
        }
    }
    info->color = org_color;

    /* ---------------------------------------- */
    /* Calculate the area of selection */
//  SDL_Rect clipped_rect = info->calcUpdatedArea(start_x, start_y,
//						  screen_ratio1, screen_ratio2);
/**/SDL_Rect clipped_rect = { int(start_x), start_y,
			      int(max_x - start_x), max_y - start_y };
    info->addShadeArea(clipped_rect, shade_distance);

    if (flush_flag)
        flush(refresh_shadow_text_mode, &clipped_rect);

    if (rect) *rect = clipped_rect;
}


void PonscripterLabel::publishTextPlane(SDL_Rect &changed, bool restart)
{
#ifdef __LIBRETRO__
    /* Cheap early-out: nothing to do unless a shell asked for the plane. */
    if (!vnovel_text_plane_wanted() || !text_info.image_surface) return;

    SDL_Surface *surf = text_info.image_surface;
    SDL_Rect r = changed;

    /* A page restarted — either the caller says so or clearCurrentTextBuffer()
     * marked it — means everything previously on the plane is gone, so the
     * whole surface counts as changed and the consumer returns to the top. */
    if (restart || text_plane_pending_restart) {
        restart = true;
        text_plane_pending_restart = false;
        r.x = r.y = 0;
        r.w = surf->w;
        r.h = surf->h;
    }
    if (r.w <= 0 || r.h <= 0) return;

    /* Clamp the reported change to the surface. */
    if (r.x < 0) { r.w += r.x; r.x = 0; }
    if (r.y < 0) { r.h += r.y; r.y = 0; }
    if (r.x + r.w > surf->w) r.w = surf->w - r.x;
    if (r.y + r.h > surf->h) r.h = surf->h - r.y;
    if (r.w <= 0 || r.h <= 0) return;

    VNovelTextPlane plane;
    plane.pixels    = surf->pixels;
    plane.width     = surf->w;
    plane.height    = surf->h;
    plane.pitch     = surf->pitch;
    plane.changed_x = r.x;
    plane.changed_y = r.y;
    plane.changed_w = r.w;
    plane.changed_h = r.h;
    /* Text origin, in screen pixels: Fontinfo holds the window origin in script
     * space (top_x/top_y) and drawChar scales it by screen_ratio1/screen_ratio2
     * before drawing, so scale the same way here for a shell that does not
     * scroll to place the text where the game drew it. */
    plane.origin_x  = sentence_font.top_x * screen_ratio1 / screen_ratio2;
    plane.origin_y  = sentence_font.top_y * screen_ratio1 / screen_ratio2;
    plane.restart   = restart ? 1 : 0;
    plane.serial    = ++text_plane_serial;
    vnovel_publish_text_plane(&plane);
#else
    (void)changed;
    (void)restart;
#endif
}

void PonscripterLabel::restoreTextBuffer()
{
    text_info.fill(0, 0, 0, 0);
    /* Replaying a page redraws every glyph: suppress the per-glyph publishes
     * and emit a single restart publish when the page is whole again. */
    text_plane_suppress = true;

    Fontinfo f_info = sentence_font;
    f_info.clear();
    const char* buffer = current_text_buffer->contents;
    int buffer_count = current_text_buffer->contents.length();

    const wchar first_ch = file_encoding->DecodeWithLigatures(buffer, f_info);
    if (is_indent_char(first_ch)) f_info.SetIndent(first_ch);

    int i = 0;
    while (i < buffer_count) {
        if (buffer[i] == 0x0a) {
            f_info.newLine();
	    ++i;
        }
        else {
            i += drawChar(buffer + i, &f_info, false, false, NULL, &text_info);
        }
    }

    /* The page is redrawn: publish it once as a restart. */
    text_plane_suppress = false;
    {
        SDL_Rect whole = { 0, 0, 0, 0 };
        publishTextPlane(whole, true);
    }
}


int PonscripterLabel::enterTextDisplayMode(bool text_flag)
{
    if (line_enter_status <= 1 && saveon_flag && internal_saveon_flag &&
	text_flag) {
        saveSaveFile(-1);
        internal_saveon_flag = false;
    }

    did_leavetext = false;
    if (!(display_mode & TEXT_DISPLAY_MODE)) {
        if (event_mode & EFFECT_EVENT_MODE) {
            if (doEffect(window_effect, false) == RET_CONTINUE) {
                display_mode = TEXT_DISPLAY_MODE;
                text_on_flag = true;
                return RET_CONTINUE | RET_NOREAD;
            }
            return RET_WAIT | RET_REREAD;
        }
        else {
            dirty_rect.clear();
            dirty_rect.add(sentence_font_info.pos);
            refreshSurface(effect_dst_surface, NULL, refresh_shadow_text_mode);

            return setEffect(window_effect, false, true);
        }
    }

    return RET_NOMATCH;
}


int PonscripterLabel::leaveTextDisplayMode(bool force_leave_flag)
{
    if (!force_leave_flag && (skip_flag || draw_one_page_flag || ctrl_pressed_status)) {
        did_leavetext = true;
        return RET_NOMATCH;
    }
    if (force_leave_flag) did_leavetext = false;

    if (!did_leavetext && (display_mode & TEXT_DISPLAY_MODE) &&
        (force_leave_flag || (erase_text_window_mode != 0))) {

        if (event_mode & EFFECT_EVENT_MODE) {
            if (doEffect(window_effect, false) == RET_CONTINUE) {
                display_mode = NORMAL_DISPLAY_MODE;
                return RET_CONTINUE | RET_NOREAD;
            }

            return RET_WAIT | RET_REREAD;
        }
        else {
            dirty_rect.add(sentence_font_info.pos);
            refreshSurface(backup_surface, &dirty_rect.bounding_box, REFRESH_NORMAL_MODE);
            SDL_BlitSurface(backup_surface, NULL, effect_dst_surface, NULL);
            SDL_BlitSurface(accumulation_surface, NULL, backup_surface, NULL);

            return setEffect(window_effect, false, false);
        }
    }

    return RET_NOMATCH;
}


void PonscripterLabel::doClickEnd()
{
    skip_to_wait = 0;

    if (automode_flag) {
        event_mode = WAIT_TEXT_MODE | WAIT_INPUT_MODE | WAIT_VOICE_MODE;
        if (automode_time < 0)
            startTimer(-automode_time * num_chars_in_sentence);
        else
            startTimer(automode_time);
    }
    else if (autoclick_time > 0) {
        event_mode = WAIT_SLEEP_MODE;
        startTimer(autoclick_time);
    }
    else {
        event_mode = WAIT_TEXT_MODE | WAIT_INPUT_MODE | WAIT_TIMER_MODE;
        advancePhase();
    }

    draw_cursor_flag = true;
    num_chars_in_sentence = 0;
}


int PonscripterLabel::clickWait(bool display_char)
{
    const char* c = script_h.getStrBuf(string_buffer_offset);

    if ((skip_flag || draw_one_page_flag || ctrl_pressed_status) &&
        !textgosub_label) {
        clickstr_state = CLICK_NONE;
        skip_to_wait = 0;
        int bytes;
        if (display_char)
            bytes = drawChar(c, &sentence_font, false, true,
                             accumulation_surface, &text_info);
        else {
            bytes = 1; // @, \, etc...?
            flush(refreshMode());
        }
        string_buffer_offset += bytes;
        num_chars_in_sentence = 0;
        return RET_CONTINUE | RET_NOREAD;
    }
    else {
        clickstr_state   = CLICK_WAIT;
        if (skip_to_wait || (sentence_font.wait_time == 0)) {
            skip_to_wait = 0;
            flush(refreshMode());
        }
        key_pressed_flag = false;
        if (display_char) {
            drawChar(c, &sentence_font, false, true, accumulation_surface,
                     &text_info);
            ++num_chars_in_sentence;
        }
        if (textgosub_label) {
            const char* next_text = c + 1;
            
            saveoffCommand("saveoff");
            textgosub_clickstr_state =
                (next_text[0] == 0x0a)
                ? CLICK_WAITEOL : CLICK_WAIT;

            gosubDoTextgosub();
            
            indent_offset = 0;        // Do we want to reset all these?
            line_enter_status = 0;
            string_buffer_offset = 0;
            return RET_CONTINUE;
        }

        doClickEnd();

        return RET_WAIT | RET_NOREAD;
    }
}


int PonscripterLabel::clickNewPage(bool display_char)
{
    const char* c = script_h.getStrBuf(string_buffer_offset);

    clickstr_state = CLICK_NEWPAGE;

    if (display_char) {
        drawChar(c, &sentence_font, true, true, accumulation_surface,
                 &text_info);
        ++num_chars_in_sentence;
    }
    
    if (skip_flag || draw_one_page_flag || skip_to_wait ||
        ctrl_pressed_status || (sentence_font.wait_time == 0))
        flush(refreshMode());

    skip_to_wait = 0;

    if ((skip_flag || ctrl_pressed_status) && !textgosub_label) {
        event_mode = WAIT_SLEEP_MODE;
        advancePhase();
        num_chars_in_sentence = 0;
    }
    else {
        key_pressed_flag = false;
        if (textgosub_label) {
            saveoffCommand("saveoff");

            textgosub_clickstr_state = CLICK_NEWPAGE;
            gosubReal(textgosub_label, script_h.getNext());
            indent_offset = 0;
            line_enter_status = 0;
            string_buffer_offset = 0;
            return RET_CONTINUE;
        }

        doClickEnd();
    }

    return RET_WAIT | RET_NOREAD;
}


int PonscripterLabel::textCommand()
{
    if (pretextgosub_label
        && (line_enter_status == 0
            || (line_enter_status == 1
                && (script_h.readStrBuf(string_buffer_offset) == '['
                    || (zenkakko_flag && file_encoding->DecodeChar(script_h.getStrBuf(string_buffer_offset)) == 0x3010 /* left lenticular bracket */))))) {
        gosubDoPretextgosub();
        line_enter_status = 1;
        return RET_CONTINUE;
    }

    int ret = enterTextDisplayMode();
    if (ret != RET_NOMATCH) return ret;

    line_enter_status = 2;
    ret = processText();
    if (ret == RET_CONTINUE) {
        indent_offset = 0;
    }

    return ret;
}


int PonscripterLabel::processText()
{
    if (string_buffer_restore > 0) {
        string_buffer_offset = string_buffer_restore;
        string_buffer_restore = -1;
    }
    if (debug_level > 1) {
        fprintf(stderr,"processText: %d:'%s", string_buffer_offset, script_h.getStrBuf(string_buffer_offset));
    }
    if (event_mode & (WAIT_INPUT_MODE | WAIT_SLEEP_MODE)) {
        draw_cursor_flag = false;
        if (clickstr_state == CLICK_WAIT) {
            string_buffer_offset += file_encoding->NextCharSizeWithLigatures
                (script_h.getStrBuf(string_buffer_offset), &sentence_font);
            clickstr_state = CLICK_NONE;
        }
        else if (clickstr_state == CLICK_NEWPAGE) {
            event_mode = IDLE_EVENT_MODE;
            string_buffer_offset += file_encoding->NextCharSizeWithLigatures
                (script_h.getStrBuf(string_buffer_offset), &sentence_font);
            newPage(true);
            clickstr_state = CLICK_NONE;
            return RET_CONTINUE | RET_NOREAD;
        }
        else if (script_h.readStrBuf(string_buffer_offset) == '!') {
            string_buffer_offset++;
            if (script_h.readStrBuf(string_buffer_offset) == 'w' ||
                script_h.readStrBuf(string_buffer_offset) == 'd') {
                ++string_buffer_offset;
                while (script_h.isadigit(script_h.readStrBuf(string_buffer_offset))) {
                    ++string_buffer_offset;
                }
                while (script_h.isawspace(script_h.readStrBuf(string_buffer_offset)))
                    ++string_buffer_offset;
            }
        }
        else
            string_buffer_offset +=
                file_encoding->NextCharSizeWithLigatures
                (script_h.getStrBuf(string_buffer_offset), &sentence_font);
        event_mode = IDLE_EVENT_MODE;
    }

    if (script_h.readStrBuf(string_buffer_offset) == 0x0a ||
        script_h.readStrBuf(string_buffer_offset) == 0x00) {
        indent_offset = 0; // redundant
        skip_to_wait = 0;
        return RET_CONTINUE;
    }

    new_line_skip_flag = false;

    char ch = script_h.readStrBuf(string_buffer_offset);

    if (ch == '@') { // wait for click
        return clickWait(false);
    }
    else if (ch == '\\') { // new page
        return clickNewPage(false);
    }
    else if (ch == '_') { // Ignore following forced return
        clickstr_state = CLICK_IGNORE;
        ++string_buffer_offset;
        return RET_CONTINUE | RET_NOREAD;
    }
    else if (ch == '!') {
        ++string_buffer_offset;
        if (script_h.readStrBuf(string_buffer_offset) == 's') {
            ++string_buffer_offset;
            bool in_skip = (skip_flag || ctrl_pressed_status);
            int prev_t = sentence_font.wait_time;
            if (script_h.readStrBuf(string_buffer_offset) == 'd') {
                sentence_font.wait_time = -1;
                ++string_buffer_offset;
            }
            else {
                int t = 0;
                while (script_h.isadigit(script_h.readStrBuf(string_buffer_offset))) {
                    t = t * 10 + script_h.readStrBuf(string_buffer_offset) -'0';
                    ++string_buffer_offset;
                }
                sentence_font.wait_time = t;
                while (script_h.isawspace(script_h.readStrBuf(string_buffer_offset)))
                    ++string_buffer_offset;
            }
            if (!in_skip && (prev_t == 0) && (sentence_font.wait_time != 0))
                flush(refreshMode());
        }
        else if (script_h.readStrBuf(string_buffer_offset) == 'w'
                 || script_h.readStrBuf(string_buffer_offset) == 'd') {
            bool flag = false;
            bool in_skip = (skip_flag || draw_one_page_flag || ctrl_pressed_status);
            if (script_h.readStrBuf(string_buffer_offset) == 'd')
                flag = true;

            ++string_buffer_offset;
            int tmp_string_buffer_offset = string_buffer_offset;
            int t = 0;
            while (script_h.isadigit(script_h.readStrBuf(string_buffer_offset))) {
                t = t * 10 + script_h.readStrBuf(string_buffer_offset) - '0';
                ++string_buffer_offset;
            }
            while (script_h.isawspace(script_h.readStrBuf(string_buffer_offset)))
                string_buffer_offset++;
            flush(refreshMode());
            if (flag && in_skip) {
                skip_to_wait = 0;
                return RET_CONTINUE | RET_NOREAD;
            }
            else {
                if (!flag && in_skip) {
                    //Mion: instead of skipping entirely, let's do a shortened wait (safer)
                    if (t > 100) {
                        t = t / 10;
                    } else if (t > 10) {
                        t = 10;
                    }
                }
                skip_to_wait = 0;
                event_mode = WAIT_SLEEP_MODE;
                if (flag) event_mode |= WAIT_INPUT_MODE;

                key_pressed_flag = false;
                startTimer(t);
                string_buffer_offset = tmp_string_buffer_offset - 2;
                return RET_WAIT | RET_NOREAD;
            }
        }
        else {
            --string_buffer_offset;
            goto notacommand;
        }

        return RET_CONTINUE | RET_NOREAD;
    }
    else if (ch == '#') {
        char hexchecker;
        for (int i = 0; i <= 5; ++i) {
            hexchecker = script_h.readStrBuf(string_buffer_offset + i + 1);
            if (!script_h.isaxdigit(hexchecker))
                goto notacommand;
        }

        sentence_font.color
            = readColour(script_h.getStrBuf(string_buffer_offset));
        string_buffer_offset += 7;

        return RET_CONTINUE | RET_NOREAD;
    }
    else if (ch == '/') {
        if (script_h.readStrBuf(string_buffer_offset + 1) != 0x0a)
            goto notacommand;
        else { // skip new line
            new_line_skip_flag = true;
            string_buffer_offset++;
            return RET_CONTINUE; // skip the following eol
        }
    }
    else {
        notacommand:

        if (clickstr_state == CLICK_IGNORE) {
            clickstr_state = CLICK_NONE;
        }
        else {
            const char* c = script_h.getStrBuf(string_buffer_offset);
            if (script_h.checkClickstr(c)) {
                if (sentence_font.isNoRoomForLines(clickstr_line))
                    return clickNewPage(true);
                else
                    return clickWait(true);
            }
        }

        bool flush_flag = !(skip_flag || draw_one_page_flag ||
                            skip_to_wait || ctrl_pressed_status ||
                            (sentence_font.wait_time == 0));

        drawChar(script_h.getStrBuf(string_buffer_offset), &sentence_font,
                 flush_flag, true, accumulation_surface, &text_info);
        ++num_chars_in_sentence;

        if (flush_flag) {
            event_mode = WAIT_SLEEP_MODE;
            int wait_time = 0;
            if ( sentence_font.wait_time == -1 )
                wait_time = default_text_speed[text_speed_no];
            else
                wait_time = sentence_font.wait_time;
            advancePhase(wait_time * 100 / global_speed_modifier);
            return RET_WAIT | RET_NOREAD;
        }
        event_mode = IDLE_EVENT_MODE;
        //Mion: hack using RET_CONTINUE | RET_WAIT for unflushed text
        return RET_CONTINUE | RET_WAIT | RET_NOREAD;
    }

    return RET_NOMATCH;
}

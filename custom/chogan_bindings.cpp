// chogan_bindings.cpp - customization layer.

// TOP

// TODO(chogan): Missing functionality
// - project wide search
//   - Scroll results when highlighted line goes off the screen
// - Auto insert matching {[("'
// - :s/.../.../g
// - % to jump to matching {[("'
// - Layouts
// - Vim style search in buffer
// - Jump to panel by number (" w1", " w3", etc.)
// - Syntax highlighting for variable defs, enums, func decl, args
// - Keep minibuffer open for displaying messages?
// - [[ or gp
// - cjh_fill_paragraph
// - Use draw_line_highlight for visual line mode
// - Cut cursor in half during multi key command

// TODO(chogan): Bugs
// - 'e' doesn't work in comments
// - Show whitespace doesn't work

#if !defined(FCODER_CHOGAN_BINDINGS_CPP)
#define FCODER_CHOGAN_BINDINGS_CPP

#include <assert.h>
#include <string.h>

#include "4coder_base_types.h"

enum CjhCommandMode
{
    CjhCommandMode_Normal,
    CjhCommandMode_Insert,
    CjhCommandMode_VisualLine,
    CjhCommandMode_Visual
};

enum CjhDir
{
    CjhDir_Forward,
    CjhDir_Backward,
};

typedef void CjhMultiKeyCmdHook(Application_Links *app);

struct CjhMultiKeyCmdHooks
{
    CjhMultiKeyCmdHook *cjh_buffer_hook;
    CjhMultiKeyCmdHook *cjh_c_hook;
    CjhMultiKeyCmdHook *cjh_comma_hook;
    CjhMultiKeyCmdHook *cjh_d_hook;
    CjhMultiKeyCmdHook *cjh_file_hook;
    CjhMultiKeyCmdHook *cjh_g_hook;
    CjhMultiKeyCmdHook *cjh_help_hook;
    CjhMultiKeyCmdHook *cjh_macro_hook;
    CjhMultiKeyCmdHook *cjh_quit_hook;
    CjhMultiKeyCmdHook *cjh_space_hook;
    CjhMultiKeyCmdHook *cjh_snippet_hook;
    CjhMultiKeyCmdHook *cjh_toggle_hook;
    CjhMultiKeyCmdHook *cjh_visual_line_mode_hook;
    CjhMultiKeyCmdHook *cjh_visual_mode_hook;
    CjhMultiKeyCmdHook *cjh_window_hook;
    CjhMultiKeyCmdHook *cjh_y_hook;
};

static u8 cjh_last_f_search;
static bool cjh_last_find_was_til;
static bool cjh_recording_command;
static bool cjh_replaying_command;
static u64 cjh_last_time_f_press;
static u64 cjh_fd_escape_delay_us = 333 * 1000;
static i64 cjh_prev_visual_line_number;
// NOTE(cjh): The number of events to subtract from a recorded macro. This is 2
// times the number of key strokes to stop a macro minus 1.
static i64 cjh_stop_recording_macro_event_count;
static CjhCommandMode cjh_command_mode;
static CjhDir cjh_last_find_dir;
static CjhMultiKeyCmdHooks cjh_multi_key_cmd_hooks;
static Range_i64 cjh_visual_line_mode_range;
static Range_i64 cjh_last_command_range;
static u64 cjh_interactive_search_selected_line;
static u8 cjh_interactive_search_string[64];

static bool cjh_in_visual_line_mode();
static bool cjh_in_visual_mode();

#include "4coder_default_include.cpp"

struct MarkNode
{
    i64 pos;
    Buffer_ID buffer;
};

#define CJH_MARK_RING_SIZE 128

struct MarkRing
{
    u32 count;
    MarkNode nodes[CJH_MARK_RING_SIZE];
};

static MarkRing cjh_mark_ring;
static Buffer_Identifier cjh_status_buffer = buffer_identifier(string_u8_litexpr("*status*"));
static View_ID cjh_status_footer_panel_view_id;
static View_ID cjh_search_panel_view_id;

// MarkRing
static void cjh_push_mark(Application_Links *app)
{
    MarkNode *mark = &cjh_mark_ring.nodes[cjh_mark_ring.count++ % ArrayCount(cjh_mark_ring.nodes)];
    View_ID view = get_active_view(app, Access_ReadWrite);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWrite);
    i64 pos = view_get_cursor_pos(app, view);
    mark->pos = pos;
    mark->buffer = buffer;
}

CUSTOM_COMMAND_SIG(cjh_pop_mark)
{
    View_ID view = get_active_view(app, Access_ReadWrite);
    i64 cursor = view_get_cursor_pos(app, view);

    for (;cjh_mark_ring.count > 0;)
    {
        MarkNode *mark = &cjh_mark_ring.nodes[--cjh_mark_ring.count % ArrayCount(cjh_mark_ring.nodes)];
        if (mark->pos == cursor)
        {
            continue;
        }
        else
        {
            view_set_buffer(app, view, mark->buffer, 0);
            view_set_cursor(app, view, seek_pos(mark->pos));
            break;
        }
    }
}

// Forward declarations
static void cjh_set_command_map(Application_Links *app, Command_Map_ID new_mapid);
static void cjh_write_key_to_status_panel(Application_Links *app);

static void cjh_toggle_highlight_range()
{
    global_config.highlight_range = !global_config.highlight_range;
}

static void cjh_begin_visual_line_mode_range(Application_Links *app)
{
    View_ID view = get_active_view(app, Access_ReadWrite);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWrite);
    i64 saved_cursor = view_get_cursor_pos(app, view);
    seek_beginning_of_line(app);
    cjh_visual_line_mode_range.start = view_get_cursor_pos(app, view);
    seek_end_of_line(app);
    cjh_visual_line_mode_range.end = view_get_cursor_pos(app, view);
    cjh_prev_visual_line_number = get_line_number_from_pos(app, buffer, saved_cursor);
    view_set_cursor(app, view, seek_pos(saved_cursor));
}

static void cjh_update_visual_line_mode_range(Application_Links *app)
{
    View_ID view = get_active_view(app, Access_ReadWrite);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWrite);
    i64 saved_cursor = view_get_cursor_pos(app, view);
    seek_beginning_of_line(app);
    i64 start = view_get_cursor_pos(app, view);
    seek_end_of_line(app);
    i64 end = view_get_cursor_pos(app, view);
    i64 line_number = get_line_number_from_pos(app, buffer, saved_cursor);

    if (line_number < cjh_prev_visual_line_number)
    {
        // NOTE(cjh): Moving up
        if (start < cjh_visual_line_mode_range.end)
        {
            cjh_visual_line_mode_range.start = start;
        }
        else
        {
            cjh_visual_line_mode_range.end = end;
        }
    }
    else
    {
        // NOTE(cjh): Moving down
        if (end > cjh_visual_line_mode_range.start)
        {
            cjh_visual_line_mode_range.end = end;
        }
        else
        {
            cjh_visual_line_mode_range.start = start;
        }
    }

    view_set_cursor(app, view, seek_pos(saved_cursor));
}

static void cjh_paint_tokens(Application_Links *app, Buffer_ID buffer, Text_Layout_ID text_layout_id)
{
    Scratch_Block scratch(app);
    FColor col = {0};

    Token_Array array = get_token_array_from_buffer(app, buffer);
    if (array.tokens != 0){
        Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
        i64 first_index = token_index_from_pos(&array, visible_range.first);
        Token_Iterator_Array it = token_iterator_index(0, &array, first_index);

        for (;;){
            Token *token = token_it_read(&it);

            if (token->pos >= visible_range.one_past_last){
                break;
            }
            // TODO(stefan): hack
            b32 valid = true;
            if(token->sub_kind == TokenCppKind_Dot){
                token_it_inc_all(&it);
                Token *peek = token_it_read(&it);
                String_Const_u8 token_as_string = push_token_lexeme(app, scratch, buffer, token);
                if(peek->kind == TokenBaseKind_Identifier &&
                   peek->sub_kind == TokenCppKind_Identifier){
                    //token_it_dec_all(&it);
                    valid = false;
                }
            }

            if (token->kind == TokenBaseKind_Identifier &&
                token->sub_kind == TokenCppKind_Identifier &&
                valid){

                String_Const_u8 token_as_string = push_token_lexeme(app, scratch, buffer, token);

                for(Buffer_ID buf = get_buffer_next(app, 0, Access_Always);
                    buf != 0;
                    buf = get_buffer_next(app, buf, Access_Always))
                {
                    Code_Index_File *file = code_index_get_file(buf);
                    if(file != 0){
                        for(i32 i = 0; i < file->note_array.count; i += 1){
                            Code_Index_Note *note = file->note_array.ptrs[i];
                            //b32 found =false;
                            switch(note->note_kind){
                                case CodeIndexNote_Type:{
                                    if(string_match(note->text, token_as_string, StringMatch_Exact)){
                                        Range_i64 range = { 0 };
                                        range.start= token->pos;
                                        range.end= token->pos+token->size;
                                        //paint_text_color(app, text_layout_id, range, ARGBFromID(defcolor_keyword));
                                        paint_text_color_fcolor(app, text_layout_id, range,
                                                                fcolor_id(defcolor_special_character));
                                        break;
                                    }
                                }break;
                            }
                        }
                    }
                }
            }
            if (!token_it_inc_all(&it)){
                break;
            }
        }
    }
}
// Multi key command hooks

static void cjh_start_recording_command(Application_Links *app)
{
    if (!cjh_replaying_command)
    {
        Buffer_ID buffer = get_keyboard_log_buffer(app);
        i64 start = buffer_get_size(app, buffer);
        Buffer_Cursor cursor = buffer_compute_cursor(app, buffer, seek_pos(start));
        // NOTE(cjh): Get previous line to capture the event that triggered this function
        Buffer_Cursor prev_line = buffer_compute_cursor(app, buffer, seek_line_col(cursor.line - 1, 1));
        cjh_last_command_range.start = prev_line.pos;
        cjh_recording_command = true;
    }
}

static void cjh_visual_mode_hook(Application_Links *app)
{
    cjh_toggle_highlight_range();
    set_mark(app);
    cjh_command_mode = CjhCommandMode_Visual;
}

static void cjh_visual_line_mode_hook(Application_Links *app)
{
    cjh_toggle_highlight_range();
    cjh_command_mode = CjhCommandMode_VisualLine;
    cjh_begin_visual_line_mode_range(app);
}

#define CJH_CMD_AND_PUSH_MARK(cmd)              \
    CUSTOM_COMMAND_SIG(cjh_##cmd##_push_mark)   \
    {                                           \
        cmd(app);                               \
        cjh_enter_normal_mode(app);             \
    }

#define CJH_START_MULTI_KEY_CMD(category)                        \
    CUSTOM_COMMAND_SIG(cjh_start_multi_key_cmd_##category)       \
    {                                                            \
        cjh_set_command_map(app, cjh_mapid_##category);          \
        if (cjh_multi_key_cmd_hooks.cjh_##category##_hook)       \
        {                                                        \
            cjh_multi_key_cmd_hooks.cjh_##category##_hook(app);  \
        }                                                        \
        cjh_write_key_to_status_panel(app);                      \
    }

// Multi key modes
CJH_START_MULTI_KEY_CMD(buffer)
CJH_START_MULTI_KEY_CMD(c)
CJH_START_MULTI_KEY_CMD(comma)
CJH_START_MULTI_KEY_CMD(d)
CJH_START_MULTI_KEY_CMD(file)
CJH_START_MULTI_KEY_CMD(g)
CJH_START_MULTI_KEY_CMD(help)
CJH_START_MULTI_KEY_CMD(macro)
CJH_START_MULTI_KEY_CMD(quit)
CJH_START_MULTI_KEY_CMD(snippet)
CJH_START_MULTI_KEY_CMD(space)
CJH_START_MULTI_KEY_CMD(toggle)
CJH_START_MULTI_KEY_CMD(visual_line_mode)
CJH_START_MULTI_KEY_CMD(visual_mode)
CJH_START_MULTI_KEY_CMD(window)
CJH_START_MULTI_KEY_CMD(y)

// Helpers

static void cjh_draw_search_buffer_token_colors(Application_Links *app, Text_Layout_ID text_layout_id,
                                                Buffer_ID buffer_id)
{
    Scratch_Block scratch(app);
    Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
    String_Const_u8 visible_text = push_buffer_range(app, scratch, buffer_id, visible_range);
    List_String_Const_u8 lines = string_split(scratch, visible_text, SCu8("\n").str, 1);

    paint_text_color_fcolor(app, text_layout_id, visible_range, fcolor_id(defcolor_text_default));

    u64 line_start_pos = 0;
    for (Node_String_Const_u8 *node = lines.first;
         node != 0;
         node = node->next)
    {
        // NOTE(cjh): Highlight filenames
        i64 one_past_end_of_filename = string_find_first(node->string, 0, ':');
        Range_i64 filename_range = {};
        filename_range.start = line_start_pos;
        filename_range.end = line_start_pos + one_past_end_of_filename;
        paint_text_color_fcolor(app, text_layout_id, filename_range, fcolor_id(defcolor_keyword));

        // NOTE(cjh): Highlight line numbers
        i64 one_past_line_number = string_find_first(node->string, one_past_end_of_filename + 1, ':');
        Range_i64 line_number_range = {};
        line_number_range.start = filename_range.end + 1;
        line_number_range.end = line_start_pos + one_past_line_number;
        paint_text_color_fcolor(app, text_layout_id, line_number_range, fcolor_id(defcolor_special_character));

        // NOTE(cjh): Highlight search term
        String_Const_u8 needle = SCu8(cjh_interactive_search_string);
        i64 match_start_line_relative = string_find_first(node->string, needle, StringMatch_Exact);
        i64 match_start_buffer_relative = line_start_pos + match_start_line_relative;

        Range_i64 buffer_range = {};
        buffer_range.start = match_start_buffer_relative;
        buffer_range.end = match_start_buffer_relative + cstring_length(cjh_interactive_search_string);
        paint_text_color_fcolor(app, text_layout_id, buffer_range, fcolor_id(defcolor_keyword));

        // NOTE(cjh): Highlight selected line
        if (cjh_interactive_search_selected_line > lines.node_count)
        {
            cjh_interactive_search_selected_line = lines.node_count;
        }
        draw_line_highlight(app, text_layout_id, cjh_interactive_search_selected_line, fcolor_id(defcolor_highlight));

        line_start_pos += node->string.size + 1;
    }
}

static i64 cjh_get_next_function_position(Application_Links *app, Buffer_ID buffer, i64 start_pos)
{
    i64 result = 0;
    Function_Positions position_result = {};
    Token_Array array = get_token_array_from_buffer(app, buffer);

    if (array.tokens != 0)
    {
        i64 token_index = token_index_from_pos(&array, start_pos);
        Get_Positions_Results get_positions_results = get_function_positions(app, buffer, token_index,
                                                                             &position_result, 1);
    }
    result = (array.tokens + position_result.sig_start_index)->pos;
    if (result == start_pos)
    {
        start_pos = (array.tokens + position_result.sig_end_index)->pos + 1;
        i64 token_index = token_index_from_pos(&array, start_pos);
        Get_Positions_Results get_positions_results = get_function_positions(app, buffer, token_index,
                                                                             &position_result, 1);
        result = (array.tokens + position_result.sig_start_index)->pos;
    }

    return result;
}

static void cjh_set_command_map(Application_Links *app, Command_Map_ID new_mapid)
{
    Buffer_ID buffer_id = view_get_buffer(app, get_active_view(app, Access_ReadVisible), Access_ReadVisible);
    Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
    Command_Map_ID* map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);
    *map_id_ptr = new_mapid;
}

static char cjh_get_char_from_user(Application_Links *app)
{
    User_Input in = {};
    char result = '\0';
    bool done = false;
    while (!done)
    {
        in = get_next_input(app, EventProperty_TextInsert,
                            EventProperty_Escape|EventProperty_ViewActivation);
        if (in.abort)
        {
            break;
        }

        switch(in.event.kind)
        {
            case InputEventKind_TextInsert:
            {
                String_Const_u8 string = to_writable(&in);
                result = string.str[0];
                done = true;
            } break;
            default:
                continue;
        }
    }

    return result;
}

static Buffer_ID cjh_get_active_buffer(Application_Links *app)
{
    View_ID view = get_active_view(app, Access_ReadWrite);
    Buffer_ID result = view_get_buffer(app, view, Access_ReadWrite);

    return result;
}

static Buffer_ID cjh_get_status_buffer(Application_Links *app)
{
    Buffer_ID result = get_buffer_by_name(app, string_u8_litexpr("*status*"), Access_Always);

    return result;
}

static View_ID cjh_open_footer_panel(Application_Links *app, View_ID view, f32 num_lines=14.0f){
    View_ID special_view = open_view(app, view, ViewSplit_Bottom);
    new_view_settings(app, special_view);
    Buffer_ID buffer = view_get_buffer(app, special_view, Access_Always);
    Face_ID face_id = get_face_id(app, buffer);
    Face_Metrics metrics = get_face_metrics(app, face_id);
    view_set_split_pixel_size(app, special_view, (i32)(metrics.line_height*num_lines));
    view_set_passive(app, special_view, true);

    return special_view;
}

static View_ID cjh_open_status_footer_panel(Application_Links *app)
{
    if (cjh_status_footer_panel_view_id == 0)
    {
        View_ID view = get_active_view(app, Access_Always);
        cjh_status_footer_panel_view_id = cjh_open_footer_panel(app, view, 1.3f);
        view_set_setting(app, cjh_status_footer_panel_view_id, ViewSetting_ShowFileBar, false);
        view_set_active(app, view);
    }
    return cjh_status_footer_panel_view_id;
}

static View_ID cjh_get_or_open_status_panel(Application_Links *app)
{
    View_ID view = 0;
    Buffer_ID buffer = cjh_get_status_buffer(app);
    if (buffer != 0){
        view = get_first_view_with_buffer(app, buffer);
    }
    if (view == 0){
        view = cjh_open_status_footer_panel(app);
    }
    return view;
}

static void cjh_close_status_panel(Application_Links *app)
{
    if (cjh_status_footer_panel_view_id != 0){
        Buffer_ID buffer = view_get_buffer(app, cjh_status_footer_panel_view_id, Access_ReadWrite);
        clear_buffer(app, buffer);
        view_close(app, cjh_status_footer_panel_view_id);
        cjh_status_footer_panel_view_id = 0;
    }
}

static void cjh_side_by_side_panels(Application_Links *app)
{
    String_Const_u8_Array file_names = {};
    Buffer_Identifier left = buffer_identifier(string_u8_litexpr("chogan_bindings.cpp"));
    // Buffer_Identifier right = buffer_identifier(string_u8_litexpr("*scratch*"));
    Buffer_Identifier right = buffer_identifier(string_u8_litexpr("*messages*"));
    default_4coder_side_by_side_panels(app, left, right, file_names);
}

static bool cjh_in_normal_mode()
{
    return cjh_command_mode == CjhCommandMode_Normal;
}

static bool cjh_in_visual_mode()
{
    return cjh_command_mode == CjhCommandMode_Visual;
}

static bool cjh_in_visual_line_mode()
{
    return cjh_command_mode == CjhCommandMode_VisualLine;
}

static bool cjh_in_insert_mode()
{
    return cjh_command_mode == CjhCommandMode_Insert;
}

CUSTOM_COMMAND_SIG(cjh_enter_normal_mode)
{
    cjh_command_mode = CjhCommandMode_Normal;
    cjh_set_command_map(app, cjh_mapid_normal_mode);
    cjh_close_status_panel(app);

    if (cjh_recording_command)
    {
        Buffer_ID buffer = get_keyboard_log_buffer(app);
        i64 end = buffer_get_size(app, buffer);
        cjh_last_command_range.end = end;
        cjh_recording_command = false;
    }

    // NOTE(cjh): This should go in CJH_CMD_AND_PUSH_MARK if it is ever removed
    // from here.
    cjh_push_mark(app);
    global_config.highlight_range = 0;
}

CUSTOM_COMMAND_SIG(cjh_enter_insert_mode)
{
    cjh_command_mode = CjhCommandMode_Insert;
    cjh_set_command_map(app, mapid_code);

    if (!cjh_recording_command && !cjh_replaying_command)
    {
        Buffer_ID buffer = get_keyboard_log_buffer(app);
        i64 start = buffer_get_size(app, buffer);
        Buffer_Cursor cursor = buffer_compute_cursor(app, buffer, seek_pos(start));
        // NOTE(cjh): Get previous line to capture the event to enter insert mode
        Buffer_Cursor prev_line = buffer_compute_cursor(app, buffer, seek_line_col(cursor.line - 1, 1));
        cjh_last_command_range.start = prev_line.pos;
        cjh_recording_command = true;
    }
}

CUSTOM_COMMAND_SIG(cjh_insert_mode_f)
{
    cjh_last_time_f_press = system_now_time();
    User_Input in = get_current_input(app);

    if (has_modifier(&in.event.key.modifiers, KeyCode_Shift))
    {
        write_text(app, SCu8("F"));
    }
    else
    {
        write_text(app, SCu8("f"));
    }
}

CUSTOM_COMMAND_SIG(cjh_insert_mode_d)
{
    if (system_now_time() - cjh_last_time_f_press < cjh_fd_escape_delay_us)
    {
        backspace_char(app);
        cjh_enter_normal_mode(app);
    }
    else
    {
        User_Input in = get_current_input(app);

        if (has_modifier(&in.event.key.modifiers, KeyCode_Shift))
        {
            write_text(app, SCu8("D"));
        }
        else
        {
            write_text(app, SCu8("d"));
        }
    }
}

CUSTOM_COMMAND_SIG(cjh_cut)
{
    View_ID view = get_active_view(app, Access_ReadWrite);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWrite);
    Range_i64 range = get_view_range(app, view);
    // NOTE(cjh): End should be inclusive
    range.end++;
    if (clipboard_post_buffer_range(app, 0, buffer, range))
    {
        buffer_replace_range(app, buffer, range, string_u8_empty);
    }
    cjh_enter_normal_mode(app);
}

static i64 cjh_buffer_seek_right_word(Application_Links *app, Buffer_ID buffer, i64 start) {

    i64 result = start;
    // TODO(cjh): Only works with ASCII (buffer_get_size returns num bytes, not characters)
    i64 end = buffer_get_size(app, buffer);
    u8 at = buffer_get_char(app, buffer, start);
    bool in_identifier = false;
    bool in_special = false;
    if (at == '/' && result + 2 < end && buffer_get_char(app, buffer, start + 1) == '/')
    {
        result += 2;
        in_identifier = true;
    }
    for (;result < end;)
    {
        at = buffer_get_char(app, buffer, result);
        if (character_is_whitespace(at))
        {
            if (in_identifier)
            {
                // NOTE(cjh): We've already seen a word, so the next word
                // boundary is the next non-whitespace character
                while (character_is_whitespace(buffer_get_char(app, buffer, result)))
                {
                    result++;
                }
                break;
            }
        }
        else if (!character_is_alpha_numeric(at) && at != '_')
        {
            // NOTE(cjh): Special character (!, &, |, (, etc.)
            if (result != start)
            {
                break;
            }
            in_special = true;
        }
        else if (character_is_alpha_numeric(at) || at == '_')
        {
            if (in_special)
            {
                break;
            }
            in_identifier = true;
        }
        else
        {
            assert(!"Invalid token in cjh_move_right_next_word");
        }
        result += 1;
    }

    result = clamp(0, result, end);

    return result;
}

CUSTOM_COMMAND_SIG(cjh_move_right_word)
{
    View_ID view = get_active_view(app, Access_ReadWrite);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWrite);
    i64 pos = view_get_cursor_pos(app, view);
    i64 next_word_pos = cjh_buffer_seek_right_word(app, buffer, pos);
    view_set_cursor(app, view, seek_pos(next_word_pos));
}

static i64 cjh_get_cursor_pos(Application_Links *app)
{
    View_ID view = get_active_view(app, Access_Read);
    i64 result = view_get_cursor_pos(app, view);

    return result;
}

static void cjh_draw_underbar_cursor(Application_Links *app, Text_Layout_ID layout, i64 pos, ARGB_Color color)
{
    Rect_f32 rect = text_layout_character_on_screen(app, layout, pos);
    rect.y0 = rect.y1 + 1.0f;
    draw_rectangle(app, rect, 0.f, color);
}

static void cjh_draw_underbar_cursor(Application_Links *app, Text_Layout_ID layout, i64 pos, FColor color)
{
    ARGB_Color argb = fcolor_resolve(color);
    cjh_draw_underbar_cursor(app, layout, pos, argb);
}

static void cjh_draw_cursor_mark_highlight(Application_Links *app, View_ID view_id, bool is_active_view,
                               Buffer_ID buffer, Text_Layout_ID text_layout_id, f32 roundness,
                               f32 outline_thickness)
{
    bool has_highlight_range = draw_highlight_range(app, view_id, buffer, text_layout_id, roundness);
    if (!has_highlight_range)
    {
        i64 cursor_pos = view_get_cursor_pos(app, view_id);
        i64 mark_pos = view_get_mark_pos(app, view_id);
        switch (cjh_command_mode)
        {
            case CjhCommandMode_Normal:
            {
                if (is_active_view)
                {
                    draw_character_block(app, text_layout_id, cursor_pos, roundness,
                                         fcolor_id(defcolor_normal_cursor));
                    paint_text_color_pos(app, text_layout_id, cursor_pos, fcolor_id(defcolor_at_cursor));
                    draw_character_wire_frame(app, text_layout_id, mark_pos, roundness, outline_thickness,
                                              fcolor_id(defcolor_mark));
                }
                else
                {
                    draw_character_wire_frame(app, text_layout_id, mark_pos, roundness, outline_thickness,
                                              fcolor_id(defcolor_mark));
                    draw_character_wire_frame(app, text_layout_id, cursor_pos, roundness, outline_thickness,
                                              fcolor_id(defcolor_normal_cursor));
                }
            } break;

            case CjhCommandMode_Insert:
            {
                draw_character_i_bar(app, text_layout_id, cursor_pos, fcolor_id(defcolor_insert_cursor));
            } break;

            case CjhCommandMode_VisualLine:
            {
                Range_i64 range = cjh_visual_line_mode_range;
                draw_character_block(app, text_layout_id, cjh_visual_line_mode_range, 1.0f,
                                     fcolor_id(defcolor_highlight));
                cjh_draw_underbar_cursor(app, text_layout_id, cursor_pos, f_white);
            } break;

            case CjhCommandMode_Visual:
            {
                Range_i64 range = {};
                range.start = Min(cursor_pos, mark_pos);
                range.end = Max(cursor_pos, mark_pos) + 1;
                draw_character_block(app, text_layout_id, range, 1.0f, fcolor_id(defcolor_highlight));
                cjh_draw_underbar_cursor(app, text_layout_id, cursor_pos, f_white);
            } break;
        }
    }
}

#include "chogan_hooks.cpp"

CUSTOM_COMMAND_SIG(cjh_move_right_and_enter_insert_mode)
{
    move_right(app);
    cjh_enter_insert_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_move_to_end_of_word)
{
    i64 initial_pos = cjh_get_cursor_pos(app);

    while (cjh_get_cursor_pos(app) <= initial_pos + 1)
    {
        move_right_token_boundary(app);
    }
    move_left(app);
}

static Buffer_ID cjh_get_or_create_buffer(Application_Links *app, Buffer_Identifier buffer_id)
{
    Buffer_ID result = 0;
    if (buffer_id.name != 0 && buffer_id.name_len > 0){
        String_Const_u8 buffer_name = SCu8(buffer_id.name, buffer_id.name_len);
        Buffer_ID buffer_attach_id = get_buffer_by_name(app, buffer_name, Access_Always);
        if (buffer_attach_id != 0){
            result = buffer_attach_id;
        }
        else{
            buffer_attach_id = create_buffer(app, buffer_name, BufferCreate_AlwaysNew|BufferCreate_NeverAttachToFile);
            if (buffer_attach_id != 0){
                result = buffer_attach_id;
            }
        }
    }
    else{
        result = buffer_id.id;
    }
    return(result);
}

static void cjh_write_key_to_status_panel(Application_Links *app)
{
    View_ID saved_view = get_active_view(app, Access_Always);
    View_ID status_view = cjh_get_or_open_status_panel(app);
    Buffer_ID status_buffer = cjh_get_or_create_buffer(app, cjh_status_buffer);
    view_set_buffer(app, status_view, status_buffer, 0);
    view_set_active(app, status_view);

    User_Input in = get_current_input(app);
    char *key_name = key_code_name[in.event.key.code];
    write_text(app, SCu8(key_name));
    write_text(app, SCu8(" "));
    view_set_active(app, saved_view);
}

static void cjh_find_cmd(Application_Links *app, u8 target, CjhDir dir, bool til)
{
    View_ID view = get_active_view(app, Access_ReadWrite);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWrite);
    // TODO(cjh): Only works with ASCII
    i64 end = buffer_get_size(app, buffer);
    i64 starting_cursor_pos = view_get_cursor_pos(app, view);
    i64 cursor_pos = starting_cursor_pos;
    seek_beginning_of_line(app);
    i64 beg_of_line = view_get_cursor_pos(app, view);
    view_set_cursor(app, view, seek_pos(cursor_pos));

    u8 at;
    switch (dir)
    {
        case CjhDir_Forward:
        {
            cursor_pos++;
            do
            {
                at = buffer_get_char(app, buffer, cursor_pos);
                if (at == target)
                {
                    if (til)
                    {
                        if (cursor_pos - 1 == starting_cursor_pos)
                        {
                            cursor_pos++;
                            continue;
                        }
                        view_set_cursor(app, view, seek_pos(cursor_pos - 1));
                    }
                    else
                    {
                        view_set_cursor(app, view, seek_pos(cursor_pos));
                    }
                    break;
                }
                cursor_pos++;

            } while (at != '\n' && cursor_pos < end);
        } break;

        case CjhDir_Backward:
        {
            cursor_pos--;
            do
            {
                at = buffer_get_char(app, buffer, cursor_pos);
                if (at == target)
                {
                    if (til)
                    {
                        if (cursor_pos + 1 == starting_cursor_pos)
                        {
                            cursor_pos--;
                            continue;
                        }
                        view_set_cursor(app, view, seek_pos(cursor_pos + 1));
                    }
                    else
                    {
                        view_set_cursor(app, view, seek_pos(cursor_pos));
                    }
                    break;
                }
                cursor_pos--;

            } while (cursor_pos >= beg_of_line);
        } break;
    }
}

CUSTOM_COMMAND_SIG(cjh_find_forward)
{
    char target = cjh_get_char_from_user(app);
    cjh_last_f_search = target;
    cjh_last_find_dir = CjhDir_Forward;
    cjh_last_find_was_til = false;
    cjh_find_cmd(app, target, CjhDir_Forward, false);
}

CUSTOM_COMMAND_SIG(cjh_find_backward)
{
    char target = cjh_get_char_from_user(app);
    cjh_last_f_search = target;
    cjh_last_find_dir = CjhDir_Backward;
    cjh_last_find_was_til = false;
    cjh_find_cmd(app, target, CjhDir_Backward, false);
}

CUSTOM_COMMAND_SIG(cjh_find_forward_til)
{
    char target = cjh_get_char_from_user(app);
    cjh_last_f_search = target;
    cjh_last_find_dir = CjhDir_Forward;
    cjh_last_find_was_til = true;
    cjh_find_cmd(app, target, CjhDir_Forward, true);
}

CUSTOM_COMMAND_SIG(cjh_find_backward_til)
{
    char target = cjh_get_char_from_user(app);
    cjh_last_f_search = target;
    cjh_last_find_dir = CjhDir_Backward;
    cjh_last_find_was_til = true;
    cjh_find_cmd(app, target, CjhDir_Backward, true);
}

CUSTOM_COMMAND_SIG(cjh_repeat_last_find_cmd)
{
    char target = cjh_last_f_search;
    CjhDir dir = cjh_last_find_dir;
    bool til = cjh_last_find_was_til;
    cjh_find_cmd(app, target, dir, til);
}

#define CJH_COMMAND_AND_ENTER_NORMAL_MODE(cmd) \
    CUSTOM_COMMAND_SIG(cjh_##cmd)              \
    {                                          \
        cmd(app);                              \
        cjh_enter_normal_mode(app);            \
    }

#define CJH_COMMAND_AND_ENTER_INSERT_MODE(cmd) \
    CUSTOM_COMMAND_SIG(cjh_##cmd)              \
    {                                          \
        cmd(app);                              \
        cjh_enter_insert_mode(app);            \
    }

#define CJH_CMD_MAPPING_PREAMBLE(cmd_map_id)        \
    MappingScope();                                 \
    SelectMapping(mapping);                         \
    SelectMap(cmd_map_id);                          \
    Bind(cjh_enter_normal_mode, KeyCode_Escape)

// Buffer commands
CJH_COMMAND_AND_ENTER_NORMAL_MODE(kill_buffer)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(interactive_switch_buffer)

CUSTOM_COMMAND_SIG(cjh_interactive_switch_buffer_other_window)
{
    cjh_interactive_switch_buffer(app);
    swap_panels(app);
}

static void cjh_setup_buffer_mapping(Mapping *mapping, i64 buffer_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(buffer_cmd_map_id);

    Bind(cjh_interactive_switch_buffer, KeyCode_B);
    Bind(cjh_interactive_switch_buffer_other_window, KeyCode_B, KeyCode_Shift);
    Bind(cjh_kill_buffer, KeyCode_D);
    // Bind(cjh_clean_old_buffers, KeyCode_D, KeyCode_Shift );
    // Bind(cjh_get_buffer_prev, KeyCode_P);
    // Bind(cjh_get_buffer_next, KeyCode_N);
}

// Window (panel) commands
CJH_COMMAND_AND_ENTER_NORMAL_MODE(close_panel)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(change_active_panel)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(open_panel_hsplit)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(open_panel_vsplit)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(swap_panels)

static void cjh_setup_window_mapping(Mapping *mapping, i64 window_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(window_cmd_map_id);

    Bind(cjh_close_panel, KeyCode_D);
    Bind(cjh_change_active_panel, KeyCode_H);
    Bind(cjh_change_active_panel, KeyCode_L);
    Bind(cjh_open_panel_vsplit, KeyCode_ForwardSlash);
    Bind(cjh_swap_panels, KeyCode_L, KeyCode_Shift);
    Bind(cjh_swap_panels, KeyCode_H, KeyCode_Shift);
    Bind(cjh_open_panel_hsplit, KeyCode_Minus);
}

// File commands
CJH_COMMAND_AND_ENTER_NORMAL_MODE(interactive_open_or_new)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(open_in_other)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(rename_file_query)

static void cjh_setup_file_mapping(Mapping *mapping, i64 file_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(file_cmd_map_id);

    Bind(cjh_interactive_open_or_new, KeyCode_F);
    Bind(cjh_open_in_other, KeyCode_F, KeyCode_Shift);
    // Bind(cjh_open_file_read_only, KeyCode_R);
    Bind(cjh_rename_file_query, KeyCode_R, KeyCode_Shift);
}

static void cjh_setup_quit_mapping(Mapping *mapping, i64 quit_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(quit_cmd_map_id);

    BindCore(default_try_exit, CoreCode_TryExit);

    Bind(exit_4coder, KeyCode_Q);
}

// Toggle commands
CJH_COMMAND_AND_ENTER_NORMAL_MODE(toggle_line_numbers)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(toggle_show_whitespace)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(toggle_virtual_whitespace)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(toggle_filebar)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(toggle_fps_meter)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(toggle_fullscreen)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(toggle_line_wrap)

static void cjh_setup_toggle_mapping(Mapping *mapping, i64 toggle_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(toggle_cmd_map_id);

    Bind(cjh_toggle_filebar, KeyCode_B);
    Bind(cjh_toggle_line_wrap, KeyCode_L);
    Bind(cjh_toggle_line_numbers, KeyCode_N);
    Bind(cjh_toggle_fullscreen, KeyCode_S);
    Bind(cjh_toggle_fps_meter, KeyCode_T);
    Bind(cjh_toggle_virtual_whitespace, KeyCode_V);
    Bind(cjh_toggle_show_whitespace, KeyCode_W);
}

// Comma commands
CJH_COMMAND_AND_ENTER_NORMAL_MODE(build_in_build_panel)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(save)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(open_matching_file_cpp)
CJH_COMMAND_AND_ENTER_INSERT_MODE(write_note)
CJH_COMMAND_AND_ENTER_INSERT_MODE(write_todo)

static void cjh_setup_comma_mapping(Mapping *mapping, i64 comma_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(comma_cmd_map_id);

    Bind(cjh_build_in_build_panel, KeyCode_B);
    // Bind(); ",f" 'fill-paragraph)
    // Bind(); ",gb" 'c-beginning-of-defun)
    // Bind(); ",ge" 'c-end-of-defun)
    Bind(cjh_open_matching_file_cpp, KeyCode_H);
    // Bind(); ",mf" 'mark-defun)
    Bind(cjh_write_note, KeyCode_N);
    // Bind(); r" 'recompile)
    // Bind(); ",si" 'cjh-wrap-region-in-if)
    Bind(cjh_write_todo, KeyCode_T);
    Bind(cjh_save, KeyCode_W);
}

// Snippet commands
CJH_COMMAND_AND_ENTER_NORMAL_MODE(snippet_lister)

static void cjh_insert_snippet(Application_Links *app, char *snippet_name)
{
    Snippet *snippet = default_snippets;
    bool found = false;
    for (i32 i = 0; i < ArrayCount(default_snippets); i += 1, snippet += 1)
    {
        if (strcmp(snippet->name, snippet_name) == 0)
        {
            found = true;
            break;
        }
    }
    assert(found);

    View_ID view = get_active_view(app, Access_ReadWrite);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    i64 pos = view_get_cursor_pos(app, view);
    write_snippet(app, view, buffer, pos, snippet);
}

#define CJH_DEFINE_INSERT_SNIPPET_FUNC(name)      \
    CUSTOM_COMMAND_SIG(cjh_insert_snippet_##name) \
    {                                             \
        cjh_insert_snippet(app, #name);           \
        cjh_enter_insert_mode(app);               \
    }

CJH_DEFINE_INSERT_SNIPPET_FUNC(if)
CJH_DEFINE_INSERT_SNIPPET_FUNC(for)
CJH_DEFINE_INSERT_SNIPPET_FUNC(case)
CJH_DEFINE_INSERT_SNIPPET_FUNC(struct)

static bool cjh_inserting_begin_if0_comment = true;

CUSTOM_COMMAND_SIG(cjh_insert_snippet_if0)
{
    if (cjh_inserting_begin_if0_comment)
    {
        cjh_insert_snippet(app, "if0");
    }
    else
    {
        cjh_insert_snippet(app, "endif");
    }
    cjh_inserting_begin_if0_comment = !cjh_inserting_begin_if0_comment;
    cjh_enter_normal_mode(app);
}

static void cjh_setup_snippet_mapping(Mapping *mapping, i64 snippet_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(snippet_cmd_map_id);

    Bind(cjh_insert_snippet_case, KeyCode_C);
    Bind(cjh_insert_snippet_for, KeyCode_F);
    Bind(cjh_insert_snippet_if, KeyCode_I);
    Bind(cjh_snippet_lister, KeyCode_L);
    Bind(cjh_insert_snippet_struct, KeyCode_S);
    Bind(cjh_insert_snippet_if0, KeyCode_0);
}

// Macro commands
CJH_COMMAND_AND_ENTER_NORMAL_MODE(keyboard_macro_start_recording)

CUSTOM_COMMAND_SIG(cjh_keyboard_macro_finish_recording)
{
    if (!global_keyboard_macro_is_recording ||
        get_current_input_is_virtual(app)){
        return;
    }

    Buffer_ID buffer = get_keyboard_log_buffer(app);
    global_keyboard_macro_is_recording = false;
    i64 end = buffer_get_size(app, buffer);
    Buffer_Cursor cursor = buffer_compute_cursor(app, buffer, seek_pos(end));
    // NOTE(cjh): We subtract lines to get rid of the commands that stop macro
    // recording. The extra -1 is a final press for the last command
    Buffer_Cursor back_cursor = buffer_compute_cursor(
        app, buffer, seek_line_col(cursor.line - cjh_stop_recording_macro_event_count - 1, 1)
    );
    global_keyboard_macro_range.one_past_last = back_cursor.pos;
    cjh_enter_normal_mode(app);

#if 0
    Scratch_Block scratch(app);
    String_Const_u8 macro = push_buffer_range(app, scratch, buffer, global_keyboard_macro_range);
    print_message(app, string_u8_litexpr("recorded:\n"));
    print_message(app, macro);
#endif
}
static void cjh_setup_macro_mapping(Mapping *mapping, i64 macro_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(macro_cmd_map_id);


    cjh_stop_recording_macro_event_count = 4;
    Bind(cjh_keyboard_macro_start_recording, KeyCode_R);
    Bind(cjh_keyboard_macro_finish_recording, KeyCode_S);
    // Bind(cjh_display_recorded_macro, KeyCode_D);
}

// C commands

#define CJH_CHANGE_COMMAND(motion)              \
    CUSTOM_COMMAND_SIG(motion##_c)              \
    {                                           \
        set_mark(app);                          \
        motion(app);                            \
        cut(app);                               \
        cjh_enter_insert_mode(app);             \
    }

CUSTOM_COMMAND_SIG(cjh_find_forward_c)
{
    set_mark(app);
    cjh_find_forward(app);
    move_right(app);
    cut(app);
    cjh_enter_insert_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_find_backward_c)
{
    set_mark(app);
    cjh_find_backward(app);
    move_left(app);
    cut(app);
    cjh_enter_insert_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_find_forward_til_c)
{
    set_mark(app);
    cjh_find_forward_til(app);
    move_right(app);
    cut(app);
    cjh_enter_insert_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_find_backward_til_c)
{
    set_mark(app);
    cjh_find_backward_til(app);
    move_left(app);
    cut(app);
    cjh_enter_insert_mode(app);
}

CJH_CHANGE_COMMAND(cjh_move_right_word)
CJH_CHANGE_COMMAND(move_right_whitespace_boundary)
CJH_CHANGE_COMMAND(move_left_token_boundary)
CJH_CHANGE_COMMAND(move_left_whitespace_boundary)
CJH_CHANGE_COMMAND(cjh_move_to_end_of_word)


static void cjh_setup_c_mapping(Mapping *mapping, i64 c_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(c_cmd_map_id);

    Bind(move_left_token_boundary_c, KeyCode_B);
    Bind(move_left_whitespace_boundary_c, KeyCode_B, KeyCode_Shift);
    Bind(cjh_move_to_end_of_word_c, KeyCode_E);
    Bind(cjh_find_forward_c, KeyCode_F);
    Bind(cjh_find_backward_c, KeyCode_F, KeyCode_Shift);
    Bind(cjh_find_forward_til_c, KeyCode_T);
    Bind(cjh_find_backward_til_c, KeyCode_T, KeyCode_Shift);
    Bind(cjh_move_right_word_c, KeyCode_W);
    Bind(move_right_whitespace_boundary_c, KeyCode_W, KeyCode_Shift);
}

// D commands

#define CJH_DELETE_COMMAND(motion)              \
    CUSTOM_COMMAND_SIG(motion##_d)              \
    {                                           \
        set_mark(app);                          \
        motion(app);                            \
        cjh_cut(app);                           \
        cjh_enter_normal_mode(app);             \
    }

CJH_DELETE_COMMAND(cjh_move_right_word)
CJH_DELETE_COMMAND(move_right_whitespace_boundary)
CJH_DELETE_COMMAND(move_left_token_boundary)
CJH_DELETE_COMMAND(move_left_whitespace_boundary)
CJH_DELETE_COMMAND(cjh_move_to_end_of_word)
CJH_DELETE_COMMAND(cjh_find_forward)
CJH_DELETE_COMMAND(cjh_find_backward)
CJH_DELETE_COMMAND(cjh_find_forward_til)
CJH_DELETE_COMMAND(cjh_find_backward_til)

static CUSTOM_COMMAND_SIG(cjh_cut_line)
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    i64 pos = view_get_cursor_pos(app, view);
    i64 line = get_line_number_from_pos(app, buffer, pos);
    Range_i64 range = get_line_pos_range(app, buffer, line);
    range.end += 1;
    i32 size = (i32)buffer_get_size(app, buffer);
    range.end = clamp_top(range.end, size);
    if (range_size(range) == 0 ||
        buffer_get_char(app, buffer, range.end - 1) != '\n'){
        range.start -= 1;
        range.first = clamp_bot(0, range.first);
    }
    clipboard_post_buffer_range(app, 0, buffer, range);
    buffer_replace_range(app, buffer, range, string_u8_litexpr(""));
    cjh_enter_normal_mode(app);
}

static void cjh_setup_d_mapping(Mapping *mapping, i64 d_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(d_cmd_map_id);

    Bind(move_left_token_boundary_d, KeyCode_B);
    Bind(move_left_whitespace_boundary_d, KeyCode_B, KeyCode_Shift);
    Bind(cjh_cut_line, KeyCode_D);
    Bind(cjh_move_to_end_of_word_d, KeyCode_E);
    Bind(cjh_find_forward_d, KeyCode_F);
    Bind(cjh_find_backward_d, KeyCode_F, KeyCode_Shift);
    Bind(cjh_find_forward_til_d, KeyCode_T);
    Bind(cjh_find_backward_til_d, KeyCode_T, KeyCode_Shift);
    Bind(cjh_move_right_word_d, KeyCode_W);
    Bind(move_right_whitespace_boundary_d, KeyCode_W, KeyCode_Shift);
}

// G commands
CJH_COMMAND_AND_ENTER_NORMAL_MODE(goto_beginning_of_file)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(comment_line_toggle)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(list_all_locations_of_type_definition_of_identifier)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(open_file_in_quotes)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(goto_line)

CUSTOM_COMMAND_SIG(cjh_goto_next_function)
{
    View_ID view = get_active_view(app, Access_ReadWrite);
    i64 pos = view_get_cursor_pos(app, view);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWrite);

    i64 next_func_pos = cjh_get_next_function_position(app, buffer, pos);
    view_set_cursor(app, view, seek_pos(next_func_pos));
    cjh_enter_normal_mode(app);
}

static void cjh_setup_g_mapping(Mapping *mapping, i64 g_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(g_cmd_map_id);

    Bind(cjh_comment_line_toggle, KeyCode_C);
    // TODO(cjh): This doesn't really work
    Bind(cjh_list_all_locations_of_type_definition_of_identifier, KeyCode_D);
    // Bind(backward-to-word, KeyCode_E);
    Bind(cjh_open_file_in_quotes, KeyCode_F);
    Bind(cjh_goto_beginning_of_file, KeyCode_G);
    Bind(cjh_goto_line, KeyCode_L);
    Bind(cjh_goto_next_function, KeyCode_N);
    // Bind(cjh_goto_prev_function, KeyCode_P);
}

// Help commands
CJH_COMMAND_AND_ENTER_NORMAL_MODE(command_documentation)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(custom_api_documentation)

static void cjh_setup_help_mapping(Mapping *mapping, i64 help_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(help_cmd_map_id);

    Bind(cjh_custom_api_documentation, KeyCode_A);
    Bind(cjh_command_documentation, KeyCode_C);
}

// Y commands

CUSTOM_COMMAND_SIG(cjh_yank_whole_line)
{
    i64 original_pos = cjh_get_cursor_pos(app);
    seek_beginning_of_line(app);
    set_mark(app);
    seek_end_of_line(app);
    // NOTE(cjh): Move right to include the newline. We remove this newline when
    // pasting in 4coder_clipboard.cpp:paste. It just lets us know that we're
    // yanking/pasting an entire line.
    move_right(app);
    copy(app);
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    view_set_cursor(app, view, seek_pos(original_pos));
    cjh_enter_normal_mode(app);
}

#define CJH_YANK_COMMAND(motion)                \
    CUSTOM_COMMAND_SIG(motion##_y)              \
    {                                           \
        set_mark(app);                          \
        motion(app);                            \
        copy(app);                              \
        cjh_enter_normal_mode(app);             \
    }

CJH_YANK_COMMAND(cjh_move_right_word)
CJH_YANK_COMMAND(move_right_whitespace_boundary)
CJH_YANK_COMMAND(move_left_token_boundary)
CJH_YANK_COMMAND(move_left_whitespace_boundary)
CJH_YANK_COMMAND(cjh_move_to_end_of_word)
CJH_YANK_COMMAND(cjh_find_forward)
CJH_YANK_COMMAND(cjh_find_backward)
CJH_YANK_COMMAND(cjh_find_forward_til)
CJH_YANK_COMMAND(cjh_find_backward_til)

static void cjh_setup_y_mapping(Mapping *mapping, i64 y_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(y_cmd_map_id);

    Bind(move_left_token_boundary_y, KeyCode_B);
    Bind(move_left_whitespace_boundary_y, KeyCode_B, KeyCode_Shift);
    Bind(cjh_move_to_end_of_word_y, KeyCode_E);
    Bind(cjh_find_forward_y, KeyCode_F);
    Bind(cjh_find_backward_y, KeyCode_F, KeyCode_Shift);
    Bind(cjh_find_forward_til_y, KeyCode_T);
    Bind(cjh_find_backward_til_y, KeyCode_T, KeyCode_Shift);
    Bind(cjh_move_right_word_y, KeyCode_W);
    Bind(move_right_whitespace_boundary_y, KeyCode_W, KeyCode_Shift);
    Bind(cjh_yank_whole_line, KeyCode_Y);
}

CUSTOM_COMMAND_SIG(cjh_insert_newline_above)
{
    set_mark(app);
    seek_beginning_of_line(app);
    // TODO(cjh): Platform independent newline
    write_text(app, SCu8("\r\n"));
    cursor_mark_swap(app);
    cjh_enter_normal_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_insert_newline_below)
{
    set_mark(app);
    seek_end_of_line(app);
    // TODO(cjh): Platform independent newline
    write_text(app, SCu8("\r\n"));
    cursor_mark_swap(app);
    cjh_enter_normal_mode(app);
}

#define CJH_VISUAL_LINE_MODE_MOTION_COMMAND(motion)       \
    CUSTOM_COMMAND_SIG(motion##_visual_line_mode)         \
    {                                                     \
        motion(app);                                      \
        cjh_update_visual_line_mode_range(app);           \
    }

// Visual Line Mode Commands
CJH_VISUAL_LINE_MODE_MOTION_COMMAND(move_left)
CJH_VISUAL_LINE_MODE_MOTION_COMMAND(move_right)
CJH_VISUAL_LINE_MODE_MOTION_COMMAND(move_up)
CJH_VISUAL_LINE_MODE_MOTION_COMMAND(move_down)

static void cjh_delete_visual_line_mode_range(Application_Links *app)
{
    View_ID view = get_active_view(app, Access_ReadWrite);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWrite);
    buffer_replace_range(app, buffer, cjh_visual_line_mode_range, string_u8_empty);
}

CUSTOM_COMMAND_SIG(cjh_visual_line_mode_change)
{
    cjh_delete_visual_line_mode_range(app);
    cjh_enter_insert_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_visual_line_mode_delete)
{
    cjh_delete_visual_line_mode_range(app);
    cjh_enter_normal_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_visual_line_mode_yank)
{
    View_ID view = get_active_view(app, Access_ReadWrite);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWrite);
    clipboard_post_buffer_range(app, 0, buffer, cjh_visual_line_mode_range);
    cjh_enter_normal_mode(app);
}

static void cjh_setup_visual_line_mode_mapping(Mapping *mapping, i64 visual_line_mode_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(visual_line_mode_cmd_map_id);

    // Motions
    Bind(move_left_visual_line_mode, KeyCode_H);
    Bind(move_right_visual_line_mode, KeyCode_L);
    Bind(move_up_visual_line_mode, KeyCode_K);
    Bind(move_down_visual_line_mode, KeyCode_J);
    // gb
    // G
    // {
    // }
    // C-u
    // C-d
    // [[
    // ]]

    // Commands
    Bind(cjh_visual_line_mode_change, KeyCode_C);
    Bind(cjh_visual_line_mode_delete, KeyCode_D);
    Bind(cjh_visual_line_mode_yank, KeyCode_Y);
}

// Visual Mode Commands

CUSTOM_COMMAND_SIG(cjh_copy)
{
    View_ID view = get_active_view(app, Access_ReadWrite);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWrite);
    Range_i64 range = get_view_range(app, view);
    // NOTE(cjh): End should be inclusive
    range.end++;
    clipboard_post_buffer_range(app, 0, buffer, range);
    cjh_enter_normal_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_visual_mode_change)
{
    cjh_cut(app);
    cjh_enter_insert_mode(app);
}

static void cjh_setup_visual_mode_mapping(Mapping *mapping, i64 visual_mode_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(visual_mode_cmd_map_id);
    ParentMap(cjh_mapid_normal_mode);

    Bind(cjh_visual_mode_change, KeyCode_C);
    Bind(cjh_cut, KeyCode_D);
    Bind(cjh_copy, KeyCode_Y);
}

#if 0
static bool cjh_toggle_buffer_forward;
CUSTOM_COMMAND_SIG(cjh_toggle_previous_buffer)
{
    Buffer_ID new_buffer = 0;
    View_ID view = get_active_view(app, Access_Read);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Read);
    if (cjh_toggle_buffer_forward)
    {
        new_buffer = get_buffer_next(app, buffer, Access_Read);
    }
    else
    {
        new_buffer = get_buffer_prev(app, buffer, Access_Read);
    }

    if (new_buffer)
    {
        view_set_buffer(app, view, new_buffer);
        cjh_toggle_buffer_forward = !cjh_toggle_buffer_forward;
    }
    cjh_enter_normal_mode(app);
}
#endif

// Space Commands
CJH_COMMAND_AND_ENTER_NORMAL_MODE(command_lister)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(list_all_locations)

static Async_Task cjh_ag_async_task = 0;

void cjh_run_ag_async(struct Async_Context *actx, Data data)
{
    Application_Links *app = actx->app;
    View_ID view = get_active_view(app, Access_ReadWrite);
    view = get_view_next(app, view, Access_ReadVisible);

    String_Const_u8 dir = SCu8(".");
    String_u8 ag = Su8(data.data, data.size);
    exec_system_command(app, view, buffer_identifier(string_u8_litexpr("*search*")),
                        dir, SCu8(ag.str, ag.size), CLI_OverlapWithConflict | CLI_AlwaysBindToView);
}

CUSTOM_COMMAND_SIG(cjh_interactive_search_project_ag)
{
    View_ID view = get_this_ctx_view(app, Access_Always);

    if (cjh_search_panel_view_id == 0)
    {
        Panel_ID root_panel = panel_get_root(app);
        if (panel_split(app, root_panel, Dimension_Y))
        {
            Panel_ID new_panel_id = panel_get_child(app, root_panel, Side_Max);
            if (new_panel_id != 0)
            {
                cjh_search_panel_view_id = panel_get_view(app, new_panel_id, Access_Always);
                view_set_setting(app, cjh_search_panel_view_id, ViewSetting_ShowFileBar, false);
            }
        }
    }

    view_set_active(app, view);

    Query_Bar_Group group(app);
    Query_Bar bar = {};
    if (start_query_bar(app, &bar, 0) == 0)
    {
        return;
    }

    char ag_cmd[256] = "ag ";
    u64 ag_cmd_size = cstring_length(ag_cmd);
    String_u8 ag = Su8((u8 *)ag_cmd, ag_cmd_size, 256);
    u8 bar_string_space[256];
    bar.string = SCu8(bar_string_space, (u64)0);

    String_Const_u8 proj_search_str = string_u8_litexpr("Project Search: ");
    bar.prompt = proj_search_str;
    Buffer_Identifier search_identifier = buffer_identifier(string_u8_litexpr("*search*"));

    User_Input in = {};
    bool done = false;
    cjh_interactive_search_selected_line = 1;

    for (;!done;)
    {
        in = get_next_input(app, EventPropertyGroup_AnyKeyboardEvent,
                            EventProperty_Escape|EventProperty_ViewActivation);
        if (in.abort)
        {
            break;
        }

        String_Const_u8 string = to_writable(&in);
        b32 string_change = false;
        b32 backspace = false;

        if (match_key_code(&in, KeyCode_Return))
        {
            Scratch_Block scratch(app);
            Buffer_ID search_buffer = buffer_identifier_to_id(app, search_identifier);
            String_Const_u8 text = push_buffer_range(app, scratch, search_buffer, buffer_range(app, search_buffer));
            List_String_Const_u8 lines = string_split(scratch, text, SCu8("\n").str, 1);
            u64 line = 1;

            for (Node_String_Const_u8 *node = lines.first;
                 node != 0;
                 node = node->next)
            {
                if (line == cjh_interactive_search_selected_line )
                {
                    String_Const_u8 filename = node->string;
                    i64 one_past_end_of_filename = string_find_first(node->string, 0, ':');
                    filename.size = one_past_end_of_filename;

                    String_Const_u8 hot_dir = push_hot_directory(app, scratch);
                    String_u8 full_filename = Su8(hot_dir.str, hot_dir.size, hot_dir.size + filename.size);
                    string_append(&full_filename, filename);

                    Buffer_ID new_buffer = create_buffer(app, full_filename.string, BufferCreate_MustAttachToFile);

                    if (new_buffer)
                    {
                        i64 one_past_line_number = string_find_first(node->string, one_past_end_of_filename + 1, ':');
                        String_Const_u8 line_number_str = SCu8(node->string.str + filename.size + 1,
                                                               one_past_line_number - (filename.size + 1));
                        u64 line_number = string_to_integer(line_number_str, 10);

                        view_set_buffer(app, view, new_buffer, 0);
                        view_set_cursor(app, view, seek_line_col(line_number, 0));
                        cjh_push_mark(app);
                    }
                }

                line++;
            }
            done = true;
            break;
        }
        else if (match_key_code(&in, KeyCode_Backspace))
        {
            // TODO(cjh): Check for alt for word backspace
            string_change = true;
            backspace = true;
            if (bar.string.size > 0)
            {
                bar.string.size--;
            }
            if (ag.size > ag_cmd_size)
            {
                ag.size--;
            }
        }
        else if (match_key_code(&in, KeyCode_J) && has_modifier(&in.event.key.modifiers, KeyCode_Control))
        {
            cjh_interactive_search_selected_line++;
        }
        else if (match_key_code(&in, KeyCode_K) && has_modifier(&in.event.key.modifiers, KeyCode_Control))
        {
            if (cjh_interactive_search_selected_line > 1)
            {
                cjh_interactive_search_selected_line--;
            }
        }
        else if (string.str != 0 && string.size > 0)
        {
            String_u8 bar_string = Su8(bar.string, sizeof(bar_string_space));
            string_append(&bar_string, string);
            bar.string = bar_string.string;

            memcpy(cjh_interactive_search_string, bar.string.str, bar.string.size);
            cjh_interactive_search_string[bar.string.size + 1] = 0;
            string_change = true;
        }
        else
        {
            // NOTE(cjh): This will allow the same input event to be triggered
            // again, except the next time it will be a TextInsert event.
            leave_current_input_unhandled(app);
        }

        if (string_change)
        {
            if (backspace)
            {
                cjh_interactive_search_string[bar.string.size] = 0;
            }
            else
            {
                string_append(&ag, string);
            }
            cjh_interactive_search_selected_line = 1;
#if 1
            // View_ID view = get_active_view(app, Access_ReadWrite);
            // view = get_view_next(app, view, Access_ReadVisible);
            String_Const_u8 dir = SCu8(".");
            exec_system_command(app, cjh_search_panel_view_id, search_identifier, dir,
                                SCu8(ag.str, ag.size), CLI_OverlapWithConflict | CLI_AlwaysBindToView);
#else
            Data data = {};
            data.data = (u8*)ag.string.str;
            data.size = ag.size;
            // if (async_task_is_running_or_pending(&global_async_system, cjh_ag_async_task))
            // {
            //     async_task_cancel(&global_async_system, cjh_ag_async_task);
            // }
            cjh_ag_async_task = async_task_no_dep(&global_async_system, cjh_run_ag_async, data);
#endif
        }
    }

    if (cjh_search_panel_view_id)
    {
        view_close(app, cjh_search_panel_view_id);
        cjh_search_panel_view_id = 0;
    }

    cjh_enter_normal_mode(app);
}

static void cjh_setup_space_mapping(Mapping *mapping, i64 space_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(space_cmd_map_id);

    // " a"
    Bind(cjh_start_multi_key_cmd_buffer, KeyCode_B);
    // " c"
    // " d"
    // Bind(); // " en" 'compilation-next-error-function)
    Bind(cjh_start_multi_key_cmd_file, KeyCode_F);
    Bind(cjh_start_multi_key_cmd_help, KeyCode_H);
    // " i"
    // Bind(); // " jw" 'ace-jump-word-mode)
    // Bind(); // " jc" 'ace-jump-char-mode)
    // Bind(); // " jl" 'ace-jump-line-mode))
    // " k"
    // " l"
    Bind(cjh_start_multi_key_cmd_macro, KeyCode_M);
    // " n"
    // " o"
    // " p"
    Bind(cjh_start_multi_key_cmd_quit, KeyCode_Q);
    // " r"
    Bind(cjh_start_multi_key_cmd_snippet, KeyCode_S);
    Bind(cjh_start_multi_key_cmd_toggle, KeyCode_T);
    // " u"
    // " v"
    Bind(cjh_start_multi_key_cmd_window, KeyCode_W);
    // " x"
    // " y"
    // " z"
    Bind(cjh_command_lister, KeyCode_Space);
    // Bind(cjh_list_all_locations, KeyCode_ForwardSlash);
    Bind(cjh_interactive_search_project_ag, KeyCode_ForwardSlash);
    // Bind(cjh_toggle_previous_buffer, KeyCode_Tab);
    Bind(cjh_insert_newline_above, KeyCode_LeftBracket);
    Bind(cjh_insert_newline_below, KeyCode_RightBracket);
}

CUSTOM_COMMAND_SIG(cjh_eol_insert)
{
    seek_end_of_line(app);
    cjh_enter_insert_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_insert_beginning_of_line)
{
    seek_beginning_of_textual_line(app);
    cjh_enter_insert_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_delete_to_eol)
{
    set_mark(app);
    seek_end_of_line(app);
    cut(app);
    move_left(app);
}

CUSTOM_COMMAND_SIG(cjh_change_to_eol)
{
    set_mark(app);
    seek_end_of_line(app);
    cut(app);
    cjh_enter_insert_mode(app);
}


CUSTOM_COMMAND_SIG(cjh_open_newline_below)
{
    seek_end_of_line(app);
    write_text(app, SCu8("\n"));
    cjh_enter_insert_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_open_newline_above)
{
    seek_beginning_of_line(app);
    write_text(app, SCu8("\n"));
    move_up(app);
    cjh_enter_insert_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_back_to_indentation)
{
    seek_beginning_of_line(app);
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    move_past_lead_whitespace(app, view, buffer);
}

CUSTOM_COMMAND_SIG(cjh_replace_char)
{
    char replacement = cjh_get_char_from_user(app);
    if (replacement != '\0')
    {
        delete_char(app);
        write_text_input(app);
    }
}

static u8 cjh_get_character_at_cursor(Application_Links *app)
{
    View_ID view = get_active_view(app, Access_ReadWrite);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWrite);
    i64 pos = view_get_cursor_pos(app, view);
    u8 result = buffer_get_char(app, buffer, pos);

    return result;
}
CUSTOM_COMMAND_SIG(cjh_insert_semicolon_at_eol)
{
    View_ID view = get_active_view(app, Access_ReadWrite);
    i64 saved_pos = view_get_cursor_pos(app, view);
    seek_end_of_line(app);
    write_text(app, SCu8(";"));
    view_set_cursor(app, view, seek_pos(saved_pos));
}

CUSTOM_COMMAND_SIG(cjh_toggle_upper_lower)
{
    u8 at = cjh_get_character_at_cursor(app);
    u8 new_char = '\0';

    if (character_is_lower(at))
    {
        new_char = character_to_upper(at);

    }
    else if (character_is_upper(at))
    {
        new_char = character_to_lower(at);
    }

    if (new_char)
    {
        delete_char(app);
        write_text(app, SCu8(&new_char, 1));
        move_left(app);
    }
}

CUSTOM_COMMAND_SIG(cjh_repeat_last_command)
{
    Buffer_ID buffer = get_keyboard_log_buffer(app);
    Scratch_Block scratch(app, Scratch_Share);
    String_Const_u8 macro = push_buffer_range(app, scratch, buffer, cjh_last_command_range);
    keyboard_macro_play(app, macro);
    cjh_enter_normal_mode(app);

#if 0
    print_message(app, string_u8_litexpr("replayed:\n"));
    print_message(app, macro);
#endif
}

static void cjh_isearch(Application_Links *app, Scan_Direction start_scan, i64 first_pos,
                        String_Const_u8 query_init, bool case_sensitive=false)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    if (!buffer_exists(app, buffer))
    {
        return;
    }

    i64 buffer_size = buffer_get_size(app, buffer);

    Query_Bar_Group group(app);
    Query_Bar bar = {};
    if (start_query_bar(app, &bar, 0) == 0)
    {
        return;
    }

    Scan_Direction scan = start_scan;
    i64 pos = first_pos;

    u8 bar_string_space[256];
    bar.string = SCu8(bar_string_space, query_init.size);
    block_copy(bar.string.str, query_init.str, query_init.size);

    String_Const_u8 isearch_str = string_u8_litexpr("I-Search: ");
    String_Const_u8 rsearch_str = string_u8_litexpr("Reverse-I-Search: ");

    u64 match_size = bar.string.size;

    User_Input in = {};
    for (;;)
    {
        switch (scan){
            case Scan_Forward:
            {
                bar.prompt = isearch_str;
            }break;
            case Scan_Backward:
            {
                bar.prompt = rsearch_str;
            }break;
        }
        isearch__update_highlight(app, view, Ii64_size(pos, match_size));

        in = get_next_input(app, EventPropertyGroup_AnyKeyboardEvent,
                            EventProperty_Escape|EventProperty_ViewActivation);
        if (in.abort){
            break;
        }

        String_Const_u8 string = to_writable(&in);

        b32 string_change = false;
        if (match_key_code(&in, KeyCode_P) && has_modifier(&in.event.key.modifiers, KeyCode_Control))
        {
            bar.string.size = cstring_length(previous_isearch_query);
            block_copy(bar.string.str, previous_isearch_query, bar.string.size);
        }
        else if (match_key_code(&in, KeyCode_Return) ||
                 match_key_code(&in, KeyCode_Tab)){
            Input_Modifier_Set *mods = &in.event.key.modifiers;
            if (has_modifier(mods, KeyCode_Control)){
                bar.string.size = cstring_length(previous_isearch_query);
                block_copy(bar.string.str, previous_isearch_query, bar.string.size);
            }
            else{
                u64 size = bar.string.size;
                size = clamp_top(size, sizeof(previous_isearch_query) - 1);
                block_copy(previous_isearch_query, bar.string.str, size);
                previous_isearch_query[size] = 0;
                break;
            }
        }
        else if (string.str != 0 && string.size > 0){
            String_u8 bar_string = Su8(bar.string, sizeof(bar_string_space));
            string_append(&bar_string, string);
            bar.string = bar_string.string;
            string_change = true;
        }
        else if (match_key_code(&in, KeyCode_Backspace)){
            if (is_unmodified_key(&in.event)){
                u64 old_bar_string_size = bar.string.size;
                bar.string = backspace_utf8(bar.string);
                string_change = (bar.string.size < old_bar_string_size);
            }
            else if (has_modifier(&in.event.key.modifiers, KeyCode_Control)){
                if (bar.string.size > 0){
                    string_change = true;
                    bar.string.size = 0;
                }
            }
        }

        // TODO(allen): how to detect if the input corresponds to
        // a search or rsearch command, a scroll wheel command?

        b32 do_scan_action = false;
        b32 do_scroll_wheel = false;
        Scan_Direction change_scan = scan;
        if (!string_change){
            if ((match_key_code(&in, KeyCode_N) && has_modifier(&in.event.key.modifiers, KeyCode_Alt)) ||
                match_key_code(&in, KeyCode_Down))
            {
                change_scan = Scan_Forward;
                do_scan_action = true;
            }
            if ((match_key_code(&in, KeyCode_P) && has_modifier(&in.event.key.modifiers, KeyCode_Alt)) ||
                match_key_code(&in, KeyCode_Up))
            {
                change_scan = Scan_Backward;
                do_scan_action = true;
            }

#if 0
            if (in.command == mouse_wheel_scroll){
                do_scroll_wheel = true;
            }
#endif
        }

        if (string_change){
            switch (scan){
                case Scan_Forward:
                {
                    i64 new_pos = 0;
                    if (case_sensitive)
                    {
                        seek_string_forward(app, buffer, pos - 1, 0, bar.string, &new_pos);
                    }
                    else
                    {
                        seek_string_insensitive_forward(app, buffer, pos - 1, 0, bar.string, &new_pos);
                    }
                    if (new_pos < buffer_size){
                        pos = new_pos;
                        match_size = bar.string.size;
                    }
                }break;

                case Scan_Backward:
                {
                    i64 new_pos = 0;
                    if (case_sensitive)
                    {
                        seek_string_backward(app, buffer, pos + 1, 0, bar.string, &new_pos);
                    }
                    else
                    {
                        seek_string_insensitive_backward(app, buffer, pos + 1, 0, bar.string, &new_pos);
                    }
                    if (new_pos >= 0){
                        pos = new_pos;
                        match_size = bar.string.size;
                    }
                }break;
            }
        }
        else if (do_scan_action){
            scan = change_scan;
            switch (scan){
                case Scan_Forward:
                {
                    i64 new_pos = 0;
                    if (case_sensitive)
                    {
                        seek_string_forward(app, buffer, pos, 0, bar.string, &new_pos);
                    }
                    else
                    {
                        seek_string_insensitive_forward(app, buffer, pos, 0, bar.string, &new_pos);
                    }
                    if (new_pos < buffer_size){
                        pos = new_pos;
                        match_size = bar.string.size;
                    }
                }break;

                case Scan_Backward:
                {
                    i64 new_pos = 0;
                    if (case_sensitive)
                    {
                        seek_string_backward(app, buffer, pos, 0, bar.string, &new_pos);
                    }
                    else
                    {
                        seek_string_insensitive_backward(app, buffer, pos, 0, bar.string, &new_pos);
                    }
                    if (new_pos >= 0){
                        pos = new_pos;
                        match_size = bar.string.size;
                    }
                }break;
            }
        }
        else if (do_scroll_wheel){
            mouse_wheel_scroll(app);
        }
        else{
            leave_current_input_unhandled(app);
        }
    }

    view_disable_highlight_range(app, view);

    if (in.abort){
        u64 size = bar.string.size;
        size = clamp_top(size, sizeof(previous_isearch_query) - 1);
        block_copy(previous_isearch_query, bar.string.str, size);
        previous_isearch_query[size] = 0;
        view_set_cursor_and_preferred_x(app, view, seek_pos(first_pos));
    }
}

CUSTOM_COMMAND_SIG(cjh_search_forward)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    i64 pos = view_get_cursor_pos(app, view);;
    cjh_isearch(app, Scan_Forward, pos, SCu8());
    cjh_push_mark(app);
}

CUSTOM_COMMAND_SIG(cjh_search_backward)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    i64 pos = view_get_cursor_pos(app, view);;
    cjh_isearch(app, Scan_Backward, pos, SCu8());
    cjh_push_mark(app);
}


static void cjh_isearch_identifier(Application_Links *app, Scan_Direction scan)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer_id = view_get_buffer(app, view, Access_ReadVisible);
    i64 pos = view_get_cursor_pos(app, view);
    Scratch_Block scratch(app);
    Range_i64 range = enclose_pos_alpha_numeric_underscore(app, buffer_id, pos);
    String_Const_u8 query = push_buffer_range(app, scratch, buffer_id, range);
    cjh_isearch(app, scan, range.first, query);
    cjh_push_mark(app);
}

CUSTOM_COMMAND_SIG(cjh_search_identifier_forward)
{
    cjh_isearch_identifier(app, Scan_Forward);
}

CUSTOM_COMMAND_SIG(cjh_search_identifier_backward)
{
    cjh_isearch_identifier(app, Scan_Backward);
}

CJH_COMMAND_AND_ENTER_NORMAL_MODE(keyboard_macro_replay)
CJH_CMD_AND_PUSH_MARK(move_up_to_blank_line)
CJH_CMD_AND_PUSH_MARK(move_down_to_blank_line)

static void cjh_setup_normal_mode_mapping(Mapping *mapping, i64 normal_mode_id)
{
    MappingScope();
    SelectMapping(mapping);
    SelectMap(normal_mode_id);

    BindCore(default_try_exit, CoreCode_TryExit);

    BindMouseWheel(mouse_wheel_scroll);
    BindMouse(click_set_cursor_and_mark, MouseCode_Left);
    BindMouseRelease(click_set_cursor, MouseCode_Left);
    BindMouseMove(click_set_cursor_if_lbutton);

    // a-z
    Bind(cjh_move_right_and_enter_insert_mode, KeyCode_A);
    Bind(move_left_token_boundary, KeyCode_B);
    Bind(cjh_start_multi_key_cmd_c, KeyCode_C);
    Bind(cjh_start_multi_key_cmd_d, KeyCode_D);
    Bind(cjh_move_to_end_of_word, KeyCode_E);
    Bind(cjh_find_forward, KeyCode_F);
    Bind(move_left, KeyCode_H);
    Bind(cjh_start_multi_key_cmd_g, KeyCode_G);
    Bind(cjh_enter_insert_mode, KeyCode_I);
    Bind(move_down, KeyCode_J);
    Bind(move_up, KeyCode_K);
    Bind(move_right, KeyCode_L);
    Bind(cursor_mark_swap, KeyCode_M);
    Bind(goto_next_jump, KeyCode_N);
    Bind(cjh_open_newline_below, KeyCode_O);
    Bind(paste, KeyCode_P);
    // Bind(cjh_quit_isearch_highlight, KeyCode_Q);
    Bind(cjh_replace_char, KeyCode_R);
    // Bind(kmacro_start_macro_or_insert_counter, KeyCode_S);
    Bind(cjh_find_forward_til, KeyCode_T);
    Bind(undo, KeyCode_U);
    Bind(cjh_start_multi_key_cmd_visual_mode, KeyCode_V);
    Bind(cjh_move_right_word, KeyCode_W);
    Bind(delete_char, KeyCode_X);
    Bind(cjh_start_multi_key_cmd_y, KeyCode_Y);
    // Bind( , KeyCode_Z);

    Bind(cjh_start_multi_key_cmd_space, KeyCode_Space);
    Bind(cjh_start_multi_key_cmd_comma, KeyCode_Comma);
    Bind(word_complete, KeyCode_Tab);
    // Indent (formatted)

    // A-Z
    Bind(cjh_eol_insert, KeyCode_A, KeyCode_Shift);
    Bind(move_left_whitespace_boundary, KeyCode_B, KeyCode_Shift);
    Bind(cjh_change_to_eol, KeyCode_C, KeyCode_Shift);
    Bind(cjh_delete_to_eol, KeyCode_D, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_E, KeyCode_Shift);
    Bind(cjh_find_backward, KeyCode_F, KeyCode_Shift);
    Bind(goto_end_of_file, KeyCode_G, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_H, KeyCode_Shift);
    Bind(cjh_insert_beginning_of_line, KeyCode_I, KeyCode_Shift);
    // Bind(cjh_delete_indentation, KeyCode_J, KeyCode_Shift);
    // Bind(kmacro-end-or-call-macro, KeyCode_K, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_L, KeyCode_Shift);
    // Bind(move_to_window_line_top_bottom, KeyCode_M, KeyCode_Shift);
    Bind(goto_prev_jump, KeyCode_N, KeyCode_Shift);
    Bind(cjh_open_newline_above, KeyCode_O, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_P, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_Q, KeyCode_Shift);
    Bind(cjh_keyboard_macro_replay, KeyCode_R, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_S, KeyCode_Shift);
    Bind(cjh_find_backward_til, KeyCode_T, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_U, KeyCode_Shift);
    Bind(cjh_start_multi_key_cmd_visual_line_mode, KeyCode_V, KeyCode_Shift);
    Bind(move_right_whitespace_boundary, KeyCode_W, KeyCode_Shift);
    Bind(backspace_char, KeyCode_X, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_Y, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_Z, KeyCode_Shift);

    Bind(seek_beginning_of_textual_line, KeyCode_0);

    // Shift-<Special characters>
    Bind(cjh_move_up_to_blank_line_push_mark, KeyCode_LeftBracket, KeyCode_Shift);
    Bind(cjh_move_down_to_blank_line_push_mark, KeyCode_RightBracket, KeyCode_Shift);
    Bind(cjh_toggle_upper_lower, KeyCode_Tick, KeyCode_Shift);
    // !
    // @
    Bind(cjh_search_identifier_backward, KeyCode_3, KeyCode_Shift);
    Bind(seek_end_of_line, KeyCode_4, KeyCode_Shift);
    // Bind(cjh_matching_paren, KeyCode_5, KeyCode_Shift);
    Bind(cjh_back_to_indentation, KeyCode_6, KeyCode_Shift);
    // &
    Bind(cjh_search_identifier_forward, KeyCode_8, KeyCode_Shift);
    // (
    // )
    // _
    // +
    // |
    // "
    // <
    // >
    Bind(cjh_search_backward, KeyCode_ForwardSlash, KeyCode_Shift);
    // :

    // Special characters
    // `
    // -
    // =
    // backslash
    Bind(cjh_search_forward, KeyCode_ForwardSlash);
    Bind(cjh_repeat_last_find_cmd, KeyCode_Semicolon);
    // Bind(cjh_goto_mark, KeyCode_Quote);
    Bind(cjh_repeat_last_command, KeyCode_Period);

    // Control modifier
    Bind(page_down, KeyCode_D, KeyCode_Control);
    Bind(center_view, KeyCode_L, KeyCode_Control);
    Bind(cjh_pop_mark, KeyCode_O, KeyCode_Control);
    // C-r
    Bind(page_up, KeyCode_U, KeyCode_Control);
    // C-v
    Bind(set_mark, KeyCode_Space, KeyCode_Control);
    Bind(cjh_insert_semicolon_at_eol, KeyCode_Semicolon, KeyCode_Control);

    // Alt modifier
    // M-q fill-paragraph

    // TODO(chogan): Surround with ("[{'
}

static void cjh_setup_insert_mode_mapping(Mapping *mapping, i64 global_id, i64 file_id, i64 code_id){
    MappingScope();
    SelectMapping(mapping);

    SelectMap(global_id);
    BindCore(chogan_default_startup, CoreCode_Startup);
    BindCore(default_try_exit, CoreCode_TryExit);

    // Default global
    Bind(keyboard_macro_start_recording , KeyCode_U, KeyCode_Control);
    Bind(keyboard_macro_finish_recording, KeyCode_U, KeyCode_Control, KeyCode_Shift);
    Bind(keyboard_macro_replay,           KeyCode_U, KeyCode_Alt);
    Bind(change_active_panel,           KeyCode_Comma, KeyCode_Control);
    Bind(change_active_panel_backwards, KeyCode_Comma, KeyCode_Control, KeyCode_Shift);
    Bind(interactive_new,               KeyCode_N, KeyCode_Control);
    Bind(interactive_open_or_new,       KeyCode_O, KeyCode_Control);
    Bind(open_in_other,                 KeyCode_O, KeyCode_Alt);
    Bind(interactive_kill_buffer,       KeyCode_K, KeyCode_Control);
    Bind(interactive_switch_buffer,     KeyCode_I, KeyCode_Control);
    Bind(project_go_to_root_directory,  KeyCode_H, KeyCode_Control);
    Bind(save_all_dirty_buffers,        KeyCode_S, KeyCode_Control, KeyCode_Shift);
    Bind(change_to_build_panel,         KeyCode_Period, KeyCode_Alt);
    Bind(close_build_panel,             KeyCode_Comma, KeyCode_Alt);
    Bind(goto_next_jump,                KeyCode_N, KeyCode_Alt);
    Bind(goto_prev_jump,                KeyCode_N, KeyCode_Alt, KeyCode_Shift);
    Bind(build_in_build_panel,          KeyCode_M, KeyCode_Alt);
    Bind(goto_first_jump,               KeyCode_M, KeyCode_Alt, KeyCode_Shift);
    Bind(toggle_filebar,                KeyCode_B, KeyCode_Alt);
    Bind(execute_any_cli,               KeyCode_Z, KeyCode_Alt);
    Bind(execute_previous_cli,          KeyCode_Z, KeyCode_Alt, KeyCode_Shift);
    Bind(command_lister,                KeyCode_X, KeyCode_Alt);
    Bind(project_command_lister,        KeyCode_X, KeyCode_Alt, KeyCode_Shift);
    Bind(list_all_functions_current_buffer, KeyCode_I, KeyCode_Control, KeyCode_Shift);
    Bind(project_fkey_command, KeyCode_F1);
    Bind(project_fkey_command, KeyCode_F2);
    Bind(project_fkey_command, KeyCode_F3);
    Bind(project_fkey_command, KeyCode_F4);
    Bind(project_fkey_command, KeyCode_F5);
    Bind(project_fkey_command, KeyCode_F6);
    Bind(project_fkey_command, KeyCode_F7);
    Bind(project_fkey_command, KeyCode_F8);
    Bind(project_fkey_command, KeyCode_F9);
    Bind(project_fkey_command, KeyCode_F10);
    Bind(project_fkey_command, KeyCode_F11);
    Bind(project_fkey_command, KeyCode_F12);
    Bind(project_fkey_command, KeyCode_F13);
    Bind(project_fkey_command, KeyCode_F14);
    Bind(project_fkey_command, KeyCode_F15);
    Bind(project_fkey_command, KeyCode_F16);
    Bind(exit_4coder,          KeyCode_F4, KeyCode_Alt);
    BindMouseWheel(mouse_wheel_scroll);
    BindMouseWheel(mouse_wheel_change_face_size, KeyCode_Control);

    SelectMap(file_id);
    ParentMap(global_id);
    BindTextInput(write_text_input);
    Bind(cjh_insert_mode_f, KeyCode_F);
    Bind(cjh_insert_mode_d, KeyCode_D);
    BindMouse(click_set_cursor_and_mark, MouseCode_Left);
    BindMouseRelease(click_set_cursor, MouseCode_Left);
    BindCore(click_set_cursor_and_mark, CoreCode_ClickActivateView);
    BindMouseMove(click_set_cursor_if_lbutton);
    Bind(cjh_enter_normal_mode, KeyCode_Escape);
    Bind(delete_char,            KeyCode_Delete);
    Bind(backspace_char,         KeyCode_Backspace);
    Bind(move_up,                KeyCode_Up);
    Bind(move_down,              KeyCode_Down);
    Bind(move_left,              KeyCode_Left);
    Bind(move_right,             KeyCode_Right);
    Bind(seek_end_of_line,       KeyCode_End);
    Bind(seek_beginning_of_line, KeyCode_Home);
    Bind(page_up,                KeyCode_PageUp);
    Bind(page_down,              KeyCode_PageDown);
    Bind(goto_beginning_of_file, KeyCode_PageUp, KeyCode_Control);
    Bind(goto_end_of_file,       KeyCode_PageDown, KeyCode_Control);
    Bind(move_up_to_blank_line_end,        KeyCode_Up, KeyCode_Control);
    Bind(move_down_to_blank_line_end,      KeyCode_Down, KeyCode_Control);
    Bind(move_left_whitespace_boundary,    KeyCode_Left, KeyCode_Control);
    Bind(move_right_whitespace_boundary,   KeyCode_Right, KeyCode_Control);
    Bind(move_line_up,                     KeyCode_Up, KeyCode_Alt);
    Bind(move_line_down,                   KeyCode_Down, KeyCode_Alt);
    Bind(backspace_alpha_numeric_boundary, KeyCode_Backspace, KeyCode_Control);
    Bind(delete_alpha_numeric_boundary,    KeyCode_Delete, KeyCode_Control);
    Bind(snipe_backward_whitespace_or_token_boundary, KeyCode_Backspace, KeyCode_Alt);
    Bind(snipe_forward_whitespace_or_token_boundary,  KeyCode_Delete, KeyCode_Alt);
    Bind(set_mark,                    KeyCode_Space, KeyCode_Control);
    Bind(replace_in_range,            KeyCode_A, KeyCode_Control);
    Bind(copy,                        KeyCode_C, KeyCode_Control);
    Bind(delete_range,                KeyCode_D, KeyCode_Control);
    Bind(delete_line,                 KeyCode_D, KeyCode_Control, KeyCode_Shift);
    Bind(center_view,                 KeyCode_E, KeyCode_Control);
    Bind(left_adjust_view,            KeyCode_E, KeyCode_Control, KeyCode_Shift);
    Bind(search,                      KeyCode_F, KeyCode_Control);
    Bind(list_all_locations,          KeyCode_F, KeyCode_Control, KeyCode_Shift);
    Bind(list_all_substring_locations_case_insensitive, KeyCode_F, KeyCode_Alt);
    Bind(goto_line,                   KeyCode_G, KeyCode_Control);
    Bind(list_all_locations_of_selection,  KeyCode_G, KeyCode_Control, KeyCode_Shift);
    Bind(snippet_lister,              KeyCode_J, KeyCode_Control);
    Bind(kill_buffer,                 KeyCode_K, KeyCode_Control, KeyCode_Shift);
    Bind(duplicate_line,              KeyCode_L, KeyCode_Control);
    Bind(cursor_mark_swap,            KeyCode_M, KeyCode_Control);
    Bind(reopen,                      KeyCode_O, KeyCode_Control, KeyCode_Shift);
    Bind(query_replace,               KeyCode_Q, KeyCode_Control);
    Bind(query_replace_identifier,    KeyCode_Q, KeyCode_Control, KeyCode_Shift);
    Bind(query_replace_selection,     KeyCode_Q, KeyCode_Alt);
    Bind(reverse_search,              KeyCode_R, KeyCode_Control);
    Bind(save,                        KeyCode_S, KeyCode_Control);
    Bind(save_all_dirty_buffers,      KeyCode_S, KeyCode_Control, KeyCode_Shift);
    Bind(search_identifier,           KeyCode_T, KeyCode_Control);
    Bind(list_all_locations_of_identifier, KeyCode_T, KeyCode_Control, KeyCode_Shift);
    Bind(paste_and_indent,            KeyCode_V, KeyCode_Control);
    Bind(paste_next_and_indent,       KeyCode_V, KeyCode_Control, KeyCode_Shift);
    Bind(cut,                         KeyCode_X, KeyCode_Control);
    Bind(redo,                        KeyCode_Y, KeyCode_Control);
    Bind(undo,                        KeyCode_Z, KeyCode_Control);
    Bind(view_buffer_other_panel,     KeyCode_1, KeyCode_Control);
    Bind(swap_panels,                 KeyCode_2, KeyCode_Control);
    Bind(if_read_only_goto_position,  KeyCode_Return);
    Bind(if_read_only_goto_position_same_panel, KeyCode_Return, KeyCode_Shift);
    Bind(view_jump_list_with_lister,  KeyCode_Period, KeyCode_Control, KeyCode_Shift);

    SelectMap(code_id);
    ParentMap(file_id);
    BindTextInput(write_text_and_auto_indent);
    Bind(cjh_insert_mode_f, KeyCode_F);
    Bind(cjh_insert_mode_d, KeyCode_D);
    Bind(move_left_alpha_numeric_boundary,           KeyCode_Left, KeyCode_Control);
    Bind(move_right_alpha_numeric_boundary,          KeyCode_Right, KeyCode_Control);
    Bind(move_left_alpha_numeric_or_camel_boundary,  KeyCode_Left, KeyCode_Alt);
    Bind(move_right_alpha_numeric_or_camel_boundary, KeyCode_Right, KeyCode_Alt);
    Bind(cjh_insert_semicolon_at_eol, KeyCode_Semicolon, KeyCode_Control);
    Bind(word_complete,              KeyCode_Tab);
    Bind(auto_indent_range,          KeyCode_Tab, KeyCode_Control);
    Bind(auto_indent_line_at_cursor, KeyCode_Tab, KeyCode_Shift);
    Bind(word_complete_drop_down,    KeyCode_Tab, KeyCode_Shift, KeyCode_Control);
    Bind(write_block,                KeyCode_R, KeyCode_Alt);
    Bind(write_todo,                 KeyCode_T, KeyCode_Alt);
    Bind(write_note,                 KeyCode_Y, KeyCode_Alt);
    Bind(list_all_locations_of_type_definition,               KeyCode_D, KeyCode_Alt);
    Bind(list_all_locations_of_type_definition_of_identifier, KeyCode_T, KeyCode_Alt, KeyCode_Shift);
    Bind(open_long_braces,           KeyCode_LeftBracket, KeyCode_Control);
    Bind(open_long_braces_semicolon, KeyCode_LeftBracket, KeyCode_Control, KeyCode_Shift);
    Bind(open_long_braces_break,     KeyCode_RightBracket, KeyCode_Control, KeyCode_Shift);
    Bind(select_surrounding_scope,   KeyCode_LeftBracket, KeyCode_Alt);
    Bind(select_surrounding_scope_maximal, KeyCode_LeftBracket, KeyCode_Alt, KeyCode_Shift);
    Bind(select_prev_scope_absolute, KeyCode_RightBracket, KeyCode_Alt);
    Bind(select_prev_top_most_scope, KeyCode_RightBracket, KeyCode_Alt, KeyCode_Shift);
    Bind(select_next_scope_absolute, KeyCode_Quote, KeyCode_Alt);
    Bind(select_next_scope_after_current, KeyCode_Quote, KeyCode_Alt, KeyCode_Shift);
    Bind(place_in_scope,             KeyCode_ForwardSlash, KeyCode_Alt);
    Bind(delete_current_scope,       KeyCode_Minus, KeyCode_Alt);
    Bind(if0_off,                    KeyCode_I, KeyCode_Alt);
    Bind(open_file_in_quotes,        KeyCode_1, KeyCode_Alt);
    Bind(open_matching_file_cpp,     KeyCode_2, KeyCode_Alt);
    Bind(write_zero_struct,          KeyCode_0, KeyCode_Control);
}

void
custom_layer_init(Application_Links *app){
    Thread_Context *tctx = get_thread_context(app);

    // NOTE(allen): setup for default framework
    async_task_handler_init(app, &global_async_system);
    code_index_init();
    buffer_modified_set_init();
    Profile_Global_List *list = get_core_profile_list(app);
    ProfileThreadName(tctx, list, string_u8_litexpr("main"));
    initialize_managed_id_metadata(app);
    set_default_color_scheme(app);

    cjh_set_hooks(app);

    // NOTE(cjh): Multi key command hooks
    cjh_multi_key_cmd_hooks.cjh_visual_line_mode_hook = cjh_visual_line_mode_hook;
    cjh_multi_key_cmd_hooks.cjh_visual_mode_hook = cjh_visual_mode_hook;
    cjh_multi_key_cmd_hooks.cjh_c_hook = cjh_start_recording_command;
    cjh_multi_key_cmd_hooks.cjh_d_hook = cjh_start_recording_command;

    mapping_init(tctx, &framework_mapping);
    cjh_setup_insert_mode_mapping(&framework_mapping, mapid_global, mapid_file, mapid_code);

    cjh_setup_buffer_mapping(&framework_mapping, cjh_mapid_buffer);
    cjh_setup_c_mapping(&framework_mapping, cjh_mapid_c);
    cjh_setup_comma_mapping(&framework_mapping, cjh_mapid_comma);
    cjh_setup_d_mapping(&framework_mapping, cjh_mapid_d);
    cjh_setup_file_mapping(&framework_mapping, cjh_mapid_file);
    cjh_setup_g_mapping(&framework_mapping, cjh_mapid_g);
    cjh_setup_help_mapping(&framework_mapping, cjh_mapid_help);
    cjh_setup_macro_mapping(&framework_mapping, cjh_mapid_macro);
    cjh_setup_normal_mode_mapping(&framework_mapping, cjh_mapid_normal_mode);
    cjh_setup_quit_mapping(&framework_mapping, cjh_mapid_quit);
    cjh_setup_snippet_mapping(&framework_mapping, cjh_mapid_snippet);
    cjh_setup_space_mapping(&framework_mapping, cjh_mapid_space);
    cjh_setup_toggle_mapping(&framework_mapping, cjh_mapid_toggle);
    cjh_setup_visual_mode_mapping(&framework_mapping, cjh_mapid_visual_mode);
    cjh_setup_visual_line_mode_mapping(&framework_mapping, cjh_mapid_visual_line_mode);
    cjh_setup_window_mapping(&framework_mapping, cjh_mapid_window);
    cjh_setup_y_mapping(&framework_mapping, cjh_mapid_y);
}

#endif //FCODER_CHOGAN_BINDINGS

// BOTTOM

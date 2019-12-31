// chogan_bindings.cpp - customization layer.

// TOP

#if !defined(FCODER_CHOGAN_BINDINGS_CPP)
#define FCODER_CHOGAN_BINDINGS_CPP

#include <assert.h>

#include "4coder_default_include.cpp"
#include "string.h"

// Helpers
static void chogan_side_by_side_panels(Application_Links *app)
{
    String_Const_u8_Array file_names = {};
    Buffer_Identifier left = buffer_identifier(string_u8_litexpr("chogan_bindings.cpp"));
    Buffer_Identifier right = buffer_identifier(string_u8_litexpr("*scratch*"));
    default_4coder_side_by_side_panels(app, left, right, file_names);
}

enum CjhCommandMode
{
    CjhCommandMode_Normal,
    CjhCommandMode_Insert,
};

static CjhCommandMode cjh_command_mode;
static u64 cjh_last_time_f_press = 0;
static u64 cjh_fd_escape_delay_us = 500 * 1000;

static bool cjh_in_normal_mode()
{
    return cjh_command_mode == CjhCommandMode_Normal;
}

static bool cjh_in_insert_mode()
{
    return cjh_command_mode == CjhCommandMode_Insert;
}

static void cjh_set_command_map(Application_Links *app, Command_Map_ID new_mapid)
{
    Buffer_ID buffer_id = view_get_buffer(app, get_active_view(app, Access_ReadVisible), Access_ReadVisible);
    Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
    Command_Map_ID* map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);
    *map_id_ptr = new_mapid;
}

CUSTOM_COMMAND_SIG(cjh_enter_normal_mode)
{
    cjh_command_mode = CjhCommandMode_Normal;
    cjh_set_command_map(app, cjh_mapid_normal_mode);
}

CUSTOM_COMMAND_SIG(cjh_enter_insert_mode)
{
    cjh_command_mode = CjhCommandMode_Insert;
    cjh_set_command_map(app, mapid_code);
}

CUSTOM_COMMAND_SIG(cjh_insert_mode_f)
{
    cjh_last_time_f_press = system_now_time();
    write_text(app, SCu8("f"));
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
        write_text_input(app);
        write_text(app, SCu8("d"));
    }
}

static i64 cjh_buffer_seek_right_word(Application_Links *app, Buffer_ID buffer, i64 start) {

    // TODO(cjh): Doesn't handle string literals that start with a number
    // TODO(cjh): Only works with ASCII
    // TODO(cjh): Assumes we'll never have 2 identifiers in a row without other
    // symbols (may happen with macros)

    i64 result = start;
    i64 end = buffer_get_size(app, buffer);
    u8 at = buffer_get_char(app, buffer, start);
    if (at == '/')
    {
        result += 2;
    }
    bool in_identifier = false;
    for (;result < end;)
    {
        at = buffer_get_char(app, buffer, result);
        if (character_is_whitespace(at))
        {
            in_identifier = false;
        }
        else if (!character_is_alpha_numeric(at) || (character_is_base10(at) && !in_identifier))
        {
            if (result != start)
            {
                break;
            }
        }
        else if (character_is_alpha(at) || (character_is_base10(at) && in_identifier))
        {
            if (!in_identifier && result != start)
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
        at = buffer_get_char(app, buffer, result);
    }

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

#include "chogan_hooks.cpp"

CUSTOM_COMMAND_SIG(cjh_move_right_and_enter_insert_mode)
{
    move_right(app);
    cjh_enter_insert_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_move_to_end_of_word)
{
    move_right_token_boundary(app);
    move_left(app);
}

#define CJH_START_MULTI_KEY_CMD(category)                  \
    CUSTOM_COMMAND_SIG(cjh_start_multi_key_cmd_##category) \
    {                                                      \
        cjh_set_command_map(app, cjh_mapid_##category);    \
    }

#define CJH_COMMAND_AND_ENTER_NORMAL_MODE(cmd) \
    CUSTOM_COMMAND_SIG(cjh_##cmd)              \
    {                                          \
        cmd(app);                              \
        cjh_enter_normal_mode(app);            \
    }

#define CJH_CMD_MAPPING_PREAMBLE(cmd_map_id)        \
    MappingScope();                                 \
    SelectMapping(mapping);                         \
    SelectMap(cmd_map_id);                          \
    Bind(cjh_enter_normal_mode, KeyCode_Escape)

CJH_START_MULTI_KEY_CMD(space)
CJH_START_MULTI_KEY_CMD(buffer)
CJH_START_MULTI_KEY_CMD(file)
CJH_START_MULTI_KEY_CMD(window)
CJH_START_MULTI_KEY_CMD(toggle)
CJH_START_MULTI_KEY_CMD(comma)
CJH_START_MULTI_KEY_CMD(macro)
CJH_START_MULTI_KEY_CMD(snippet)
CJH_START_MULTI_KEY_CMD(quit)
CJH_START_MULTI_KEY_CMD(c)
CJH_START_MULTI_KEY_CMD(d)
CJH_START_MULTI_KEY_CMD(g)
CJH_START_MULTI_KEY_CMD(y)

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
    // -
    // =
}

// File commands
CJH_COMMAND_AND_ENTER_NORMAL_MODE(interactive_open_or_new)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(open_in_other)

static void cjh_setup_file_mapping(Mapping *mapping, i64 file_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(file_cmd_map_id);

    Bind(cjh_interactive_open_or_new, KeyCode_F);
    Bind(cjh_open_in_other, KeyCode_F, KeyCode_Shift);
    // Bind(cjh_open_file_read_only, KeyCode_R);
    // Bind(cjh_rename_file, KeyCode_R, KeyCode_Shift);
}

static void cjh_setup_quit_mapping(Mapping *mapping, i64 quit_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(quit_cmd_map_id);

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
CJH_COMMAND_AND_ENTER_NORMAL_MODE(write_note)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(write_todo)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(open_matching_file_cpp)

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
        cjh_enter_normal_mode(app);               \
    }

CJH_DEFINE_INSERT_SNIPPET_FUNC(if)
CJH_DEFINE_INSERT_SNIPPET_FUNC(for)

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

    Bind(cjh_insert_snippet_if, KeyCode_I);
    Bind(cjh_insert_snippet_for, KeyCode_F);
    Bind(cjh_snippet_lister, KeyCode_S);
    Bind(cjh_insert_snippet_if0, KeyCode_0);
}

// Macro commands
CJH_COMMAND_AND_ENTER_NORMAL_MODE(keyboard_macro_start_recording)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(keyboard_macro_finish_recording)

static void cjh_setup_macro_mapping(Mapping *mapping, i64 macro_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(macro_cmd_map_id);

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
    // f
    // F
    // t
    // T
    Bind(cjh_move_right_word_c, KeyCode_W);
    Bind(move_right_whitespace_boundary_c, KeyCode_W, KeyCode_Shift);
}

// D commands
CJH_COMMAND_AND_ENTER_NORMAL_MODE(delete_line)

#define CJH_DELETE_COMMAND(motion)              \
    CUSTOM_COMMAND_SIG(motion##_d)              \
    {                                           \
        set_mark(app);                          \
        motion(app);                            \
        cut(app);                               \
        cjh_enter_normal_mode(app);             \
    }

CJH_DELETE_COMMAND(cjh_move_right_word)
CJH_DELETE_COMMAND(move_right_whitespace_boundary)
CJH_DELETE_COMMAND(move_left_token_boundary)
CJH_DELETE_COMMAND(move_left_whitespace_boundary)
CJH_DELETE_COMMAND(cjh_move_to_end_of_word)

static void cjh_setup_d_mapping(Mapping *mapping, i64 d_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(d_cmd_map_id);

    Bind(move_left_token_boundary_d, KeyCode_B);
    Bind(move_left_whitespace_boundary_d, KeyCode_B, KeyCode_Shift);
    Bind(cjh_delete_line, KeyCode_D);
    Bind(cjh_move_to_end_of_word_d, KeyCode_E);
    // f
    // F
    // t
    // T
    Bind(cjh_move_right_word_d, KeyCode_W);
    Bind(move_right_whitespace_boundary_d, KeyCode_W, KeyCode_Shift);
}

// G commands
CJH_COMMAND_AND_ENTER_NORMAL_MODE(goto_beginning_of_file)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(comment_line_toggle)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(list_all_locations_of_type_definition_of_identifier)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(open_file_in_quotes)
CJH_COMMAND_AND_ENTER_NORMAL_MODE(goto_line)

static void cjh_setup_g_mapping(Mapping *mapping, i64 g_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(g_cmd_map_id);

    Bind(cjh_goto_beginning_of_file, KeyCode_B);
    Bind(cjh_comment_line_toggle, KeyCode_C);
    // TODO(cjh): This doesn't really work
    Bind(cjh_list_all_locations_of_type_definition_of_identifier, KeyCode_D);
    // Bind(backward-to-word, KeyCode_E);
    Bind(cjh_goto_line, KeyCode_G);
    // TODO(cjh): Doesn't end up in normal mode?
    Bind(cjh_open_file_in_quotes, KeyCode_F);
}

// Y commands

CUSTOM_COMMAND_SIG(cjh_yank_whole_line)
{
    // TODO(cjh): Doesn't insert newline when pasted
    seek_beginning_of_line(app);
    set_mark(app);
    seek_end_of_line(app);
    copy(app);
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

static void cjh_setup_y_mapping(Mapping *mapping, i64 y_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(y_cmd_map_id);

    Bind(move_left_token_boundary_y, KeyCode_B);
    Bind(move_left_whitespace_boundary_y, KeyCode_B, KeyCode_Shift);
    Bind(cjh_move_to_end_of_word_y, KeyCode_E);
    // f
    // F
    // t
    // T
    Bind(cjh_move_right_word_y, KeyCode_W);
    Bind(move_right_whitespace_boundary_y, KeyCode_W, KeyCode_Shift);
    Bind(cjh_yank_whole_line, KeyCode_Y);
}

CUSTOM_COMMAND_SIG(cjh_insert_newline_above)
{
    set_mark(app);
    seek_beginning_of_line(app);
    write_text(app, SCu8("\n"));
    cursor_mark_swap(app);
    cjh_enter_normal_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_insert_newline_below)
{
    set_mark(app);
    seek_end_of_line(app);
    write_text(app, SCu8("\n"));
    cursor_mark_swap(app);
    cjh_enter_normal_mode(app);
}

static void cjh_setup_space_mapping(Mapping *mapping, i64 space_cmd_map_id)
{
    CJH_CMD_MAPPING_PREAMBLE(space_cmd_map_id);

    // " a"
    Bind(cjh_start_multi_key_cmd_buffer, KeyCode_B);
    // " c"
    // Bind(); // " d" 'dired)
    // Bind(); // " en" 'compilation-next-error-function)
    Bind(cjh_start_multi_key_cmd_file, KeyCode_F);
    // " i"
    // Bind(); // " jw" 'ace-jump-word-mode)
    // Bind(); // " jc" 'ace-jump-char-mode)
    // Bind(); // " jl" 'ace-jump-line-mode))
    // " k"
    // Bind(); // " ls" 'window-configuration-to-register)
    // Bind(); // " ll" 'jump-to-register)
    Bind(cjh_start_multi_key_cmd_macro, KeyCode_M);
    // " n"
    // " o"
    // " p"
    Bind(cjh_start_multi_key_cmd_quit, KeyCode_Q);
    // Bind(); // " r" 'cjh-reload-init-file)
    // " ry"
    Bind(cjh_start_multi_key_cmd_snippet, KeyCode_S);
    // s/.../.../g
    Bind(cjh_start_multi_key_cmd_toggle, KeyCode_T);
    // " u"
    // " v"
    Bind(cjh_start_multi_key_cmd_window, KeyCode_W);
    // " x"
    // " y"
    // " z"
    // Bind(cjh_interactive_search_in_project, KeyCode_ForwardSlash);
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
    }
}

CJH_COMMAND_AND_ENTER_NORMAL_MODE(keyboard_macro_replay)

static void cjh_setup_normal_mode_mapping(Mapping *mapping, i64 normal_mode_id)
{
    MappingScope();
    SelectMapping(mapping);
    SelectMap(normal_mode_id);

    // a-z
    Bind(cjh_move_right_and_enter_insert_mode, KeyCode_A);
    Bind(move_left_token_boundary, KeyCode_B);
    Bind(cjh_start_multi_key_cmd_c, KeyCode_C);
    Bind(cjh_start_multi_key_cmd_d, KeyCode_D);
    // TODO(cjh): Can't do multiple 'e' in a row
    Bind(cjh_move_to_end_of_word, KeyCode_E);
    // Bind(cjh_find_forward, KeyCode_F);
    Bind(move_left, KeyCode_H);
    Bind(cjh_start_multi_key_cmd_g, KeyCode_G);
    Bind(cjh_enter_insert_mode, KeyCode_I);
    Bind(move_down, KeyCode_J);
    Bind(move_up, KeyCode_K);
    Bind(move_right, KeyCode_L);
    // Bind(cjh_store_mark, KeyCode_M);
    // Bind(cjh_isearch_next, KeyCode_N);
    Bind(cjh_open_newline_below, KeyCode_O);
    Bind(paste, KeyCode_P);
    // Bind(cjh_quit_isearch_highlight, KeyCode_Q);
    // TODO(cjh): Not quite working
    Bind(cjh_replace_char, KeyCode_R);
    // Bind(kmacro_start_macro_or_insert_counter, KeyCode_S);
    // Bind(cjh_find_forward_till, KeyCode_T);
    Bind(undo, KeyCode_U);
    // Bind(cjh_visual_state, KeyCode_V);
    // TODO(cjh): 'w' doesn't work quite right
    Bind(cjh_move_right_word, KeyCode_W);
    Bind(delete_char, KeyCode_X);
    Bind(cjh_start_multi_key_cmd_y, KeyCode_Y);
    // Bind(AVAILABLE, KeyCode_Z);

    Bind(cjh_start_multi_key_cmd_space, KeyCode_Space);
    Bind(cjh_start_multi_key_cmd_comma, KeyCode_Comma);
    // (define-key cjh-keymap (kbd "TAB") 'indent-for-tab-command)

    // A-Z
    Bind(cjh_eol_insert, KeyCode_A, KeyCode_Shift);
    Bind(move_left_whitespace_boundary, KeyCode_B, KeyCode_Shift);
    Bind(cjh_change_to_eol, KeyCode_C, KeyCode_Shift);
    Bind(cjh_delete_to_eol, KeyCode_D, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_E, KeyCode_Shift);
    // Bind(cjh_find_backward, KeyCode_F, KeyCode_Shift);
    Bind(goto_end_of_file, KeyCode_G, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_H, KeyCode_Shift);
    Bind(cjh_insert_beginning_of_line, KeyCode_I, KeyCode_Shift);
    // Bind(cjh_delete_indentation, KeyCode_J, KeyCode_Shift);
    // Bind(kmacro-end-or-call-macro, KeyCode_K, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_L, KeyCode_Shift);
    // Bind(move_to_window_line_top_bottom, KeyCode_M, KeyCode_Shift);
    // Bind(cjh_isearch_prev, KeyCode_N, KeyCode_Shift);
    Bind(cjh_open_newline_above, KeyCode_O, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_P, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_Q, KeyCode_Shift);
    // TODO(cjh): Only works once
    Bind(cjh_keyboard_macro_replay, KeyCode_R, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_S, KeyCode_Shift);
    // Bind(cjh_find_backward_till, KeyCode_T, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_U, KeyCode_Shift);
    // Bind(cjh_start_visual_line_selection, KeyCode_V, KeyCode_Shift);
    Bind(move_right_whitespace_boundary, KeyCode_W, KeyCode_Shift);
    Bind(backspace_char, KeyCode_X, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_Y, KeyCode_Shift);
    // Bind(AVAILABLE, KeyCode_Z, KeyCode_Shift);

    Bind(seek_beginning_of_textual_line, KeyCode_0);

    // Shift-<Special characters>
    Bind(seek_end_of_line, KeyCode_4, KeyCode_Shift);
    Bind(move_up_to_blank_line, KeyCode_LeftBracket, KeyCode_Shift);
    Bind(move_down_to_blank_line, KeyCode_RightBracket, KeyCode_Shift);
    // TODO(cjh): Figure out KeyCode for backtick
    Bind(cjh_toggle_upper_lower, 38, KeyCode_Shift);
    // `
    // !
    // @
    // #
    // (define-key cjh-keymap "%" 'cjh-matching-paren)
    Bind(cjh_back_to_indentation, KeyCode_6, KeyCode_Shift);
    // &
    // TODO(cjh): How is search supposed to work?
    Bind(search_identifier, KeyCode_8, KeyCode_Shift);
    // (
    // )
    // _
    // +
    // |
    // "
    // <
    // >
    // (define-key cjh-keymap "?" 'cjh-isearch-backward)
    // :

    // Special characters
    // -
    // =
    // \
    // (define-key cjh-keymap ";" 'cjh-repeat-last-find)
    // (define-key cjh-keymap "'" 'cjh-goto-mark)
    // (define-key cjh-keymap "." 'cjh-repeat-last-command)

    // Control-<a-z>
    // TODO(cjh): Put the cursor in the middle of the screen?
    Bind(page_down, KeyCode_D, KeyCode_Control);
    // C-l
    Bind(page_up, KeyCode_U, KeyCode_Control);
    // (define-key cjh-keymap (kbd "C-o") 'pop-to-mark-command)
    // C-v
    // (define-key cjh-keymap (kbd "C-;") 'cjh-insert-semicolon-at-eol)

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
    Bind(comment_line_toggle,        KeyCode_Semicolon, KeyCode_Control);
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

    // NOTE(cjh): Setup custom hooks, command maps, and keybindings.
    chogan_set_hooks(app);
    mapping_init(tctx, &framework_mapping);
    cjh_setup_insert_mode_mapping(&framework_mapping, mapid_global, mapid_file, mapid_code);
    cjh_setup_normal_mode_mapping(&framework_mapping, cjh_mapid_normal_mode);
    cjh_setup_space_mapping(&framework_mapping, cjh_mapid_space);
    cjh_setup_buffer_mapping(&framework_mapping, cjh_mapid_buffer);
    cjh_setup_file_mapping(&framework_mapping, cjh_mapid_file);
    cjh_setup_window_mapping(&framework_mapping, cjh_mapid_window);
    cjh_setup_toggle_mapping(&framework_mapping, cjh_mapid_toggle);
    cjh_setup_comma_mapping(&framework_mapping, cjh_mapid_comma);
    cjh_setup_snippet_mapping(&framework_mapping, cjh_mapid_snippet);
    cjh_setup_macro_mapping(&framework_mapping, cjh_mapid_macro);
    cjh_setup_quit_mapping(&framework_mapping, cjh_mapid_quit);
    cjh_setup_c_mapping(&framework_mapping, cjh_mapid_c);
    cjh_setup_d_mapping(&framework_mapping, cjh_mapid_d);
    cjh_setup_g_mapping(&framework_mapping, cjh_mapid_g);
    cjh_setup_y_mapping(&framework_mapping, cjh_mapid_y);
}

#endif //FCODER_CHOGAN_BINDINGS

// BOTTOM

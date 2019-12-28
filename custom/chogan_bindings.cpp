// chogan_bindings.cpp - chogan customization layer.

// TOP

#if !defined(FCODER_CHOGAN_BINDINGS_CPP)
#define FCODER_CHOGAN_BINDINGS_CPP

#include <assert.h>

#include "4coder_default_include.cpp"
#include "4coder_default_map.cpp"

enum CjhCommandMode
{
    CjhCommandMode_Normal,
    CjhCommandMode_Insert,
};

static CjhCommandMode cjh_command_mode;

static bool cjh_in_normal_mode()
{
    return cjh_command_mode == CjhCommandMode_Normal;
}

static bool cjh_in_insert_mode()
{
    return cjh_command_mode == CjhCommandMode_Insert;
}

#include "chogan_hooks.cpp"

CUSTOM_COMMAND_SIG(cjh_enter_normal_mode)
{
    cjh_command_mode = CjhCommandMode_Normal;
    Buffer_ID buffer_id = view_get_buffer(app, get_active_view(app, Access_ReadVisible), Access_ReadVisible);
    Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
    Command_Map_ID* map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);
    *map_id_ptr = mapid_normal_mode;
    // TODO(cjh): There must be a better way. render_buffer should switch on mode
    *active_color_table.arrays[defcolor_cursor].vals = 0xFFEEAD0E;
}

CUSTOM_COMMAND_SIG(cjh_enter_insert_mode)
{
    cjh_command_mode = CjhCommandMode_Insert;
    Buffer_ID buffer_id = view_get_buffer(app, get_active_view(app, Access_ReadVisible), Access_ReadVisible);
    Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
    Command_Map_ID* map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);
    *map_id_ptr = mapid_code;
    // TODO(cjh): There must be a better way.
    *active_color_table.arrays[defcolor_cursor].vals = 0xFF66CD00;
}

CUSTOM_COMMAND_SIG(cjh_move_forward_and_enter_insert_mode)
{
    move_right(app);
    cjh_enter_insert_mode(app);
}

CUSTOM_COMMAND_SIG(cjh_move_to_end_of_word)
{
    move_right_token_boundary(app);
    move_left(app);
}

static void cjh_setup_normal_mode_mapping(Mapping *mapping, i64 normal_mode_id)
{
    MappingScope();
    SelectMapping(mapping);
    SelectMap(normal_mode_id);


    Bind(cjh_move_forward_and_enter_insert_mode, KeyCode_A);
    Bind(move_left_token_boundary, KeyCode_B);
    // Bind(cjh_change, KeyCode_C);
    // Bind(cjh_delete, KeyCode_D);
    // TODO(cjh): Can't do multiple 'e' in a row
    Bind(cjh_move_to_end_of_word, KeyCode_E);
    // Bind(cjh_find_forward, KeyCode_F);
    Bind(move_left, KeyCode_H);
    Bind(cjh_enter_insert_mode, KeyCode_I);
    Bind(move_down, KeyCode_J);
    Bind(move_up, KeyCode_K);
    Bind(move_right, KeyCode_L);
    // Bind(cjh_store_mark, KeyCode_M);
    // Bind(cjh_isearch_next, KeyCode_N);
    // Bind(cjh_open_newline_below, KeyCode_O);
    Bind(paste, KeyCode_P);
    // Bind(cjh_quit_isearch_highlight, KeyCode_Q);
    // Bind(cjh_replace_char, KeyCode_R);
    // Bind(kmacro_start_macro_or_insert_counter, KeyCode_S);
    // Bind(cjh_find_forward_till, KeyCode_T);
    // Bind(undo, KeyCode_U);
    // Bind(cjh_visual_state, KeyCode_V);
    // TODO(cjh): Not really happy with 'w' behavior
    Bind(move_right_token_boundary, KeyCode_W);
    // Bind(cjh_forward_delete_char, KeyCode_X);
    Bind(copy, KeyCode_Y);
    // z

#if 0
(define-key cjh-keymap "gc" 'cjh-comment-region)
(define-key cjh-keymap "gd" 'xref-find-definitions)
(define-key cjh-keymap "ge" 'backward-to-word)
(define-key cjh-keymap "gg" 'beginning-of-buffer)

(define-key cjh-keymap "A" 'cjh-eol-insert)
(define-key cjh-keymap "B" 'cjh-backward-whitespace)
(define-key cjh-keymap "C" 'cjh-change-to-eol)
(define-key cjh-keymap "D" 'kill-line)
;; E
(define-key cjh-keymap "F" 'cjh-find-backward)
(define-key cjh-keymap "G" 'end-of-buffer)
;; H
(define-key cjh-keymap "I" 'cjh-insert-beginning-of-line)
(define-key cjh-keymap "J" 'cjh-delete-indentation)
(define-key cjh-keymap "K" 'kmacro-end-or-call-macro)
;; L
(define-key cjh-keymap "M" 'move-to-window-line-top-bottom)
(define-key cjh-keymap "N" 'cjh-isearch-prev)
(define-key cjh-keymap "O" 'cjh-open-newline-above)
;; P
;; Q
;; R
;; S
;; TODO(chogan): Implement this
(define-key cjh-keymap "T" 'cjh-find-backward-till)
;; U
;; TODO(chogan): Not quite right
(define-key cjh-keymap "V" 'cjh-start-visual-line-selection)
(define-key cjh-keymap "W" 'cjh-forward-whitespace)
(define-key cjh-keymap "X" 'cjh-backward-delete-char)
;; Y
;; Z
;; ~
;; `
;; !
;; @
;; #
(define-key cjh-keymap "$" 'cjh-move-to-end-of-line)
(define-key cjh-keymap "%" 'cjh-matching-paren)
(define-key cjh-keymap "^" 'back-to-indentation)
;; &
(define-key cjh-keymap "*" 'isearch-forward-symbol-at-point)
;; *e
;; (
;; )
;; -
;; _
;; +
;; =
;; \
;; |
(define-key cjh-keymap ";" 'cjh-repeat-last-find)
(define-key cjh-keymap "'" 'cjh-goto-mark)
;; :
;; "
(define-key cjh-keymap "{" 'backward-paragraph)
(define-key cjh-keymap "}" 'forward-paragraph)
(define-key cjh-keymap "[ " 'cjh-newline-above)
(define-key cjh-keymap "] " 'cjh-newline-below)
;; <
;; >
;; TODO(chogan): Improve semantics of this
(define-key cjh-keymap "." 'cjh-repeat-last-command)

(if cjh-use-swiper
    (define-key cjh-keymap "/" 'swiper)
  (define-key cjh-keymap "/" 'cjh-isearch-forward))

(define-key cjh-keymap "?" 'cjh-isearch-backward)
(define-key cjh-keymap "0" 'beginning-of-line)
(define-key cjh-keymap (kbd "TAB") 'indent-for-tab-command)

(define-key cjh-keymap (kbd "C-d") 'cjh-scroll-up-half)
;; C-n 'cjh-multi-cursor-add
(define-key cjh-keymap (kbd "C-o") 'pop-to-mark-command)
(define-key cjh-keymap (kbd "C-u") 'cjh-scroll-down-half)
;; C-v
(define-key cjh-keymap (kbd "C-;") 'cjh-insert-semicolon-at-eol)
(define-key cjh-keymap (kbd "M-K") 'apply-macro-to-region-lines)
#endif
}

static void cjh_setup_insert_mode_mapping(Mapping *mapping, i64 global_id, i64 file_id, i64 code_id){
    MappingScope();
    SelectMapping(mapping);

    SelectMap(global_id);
    BindCore(default_startup, CoreCode_Startup);
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


    // NOTE(allen): default hooks and command maps
    chogan_set_hooks(app);
    mapping_init(tctx, &framework_mapping);
    cjh_setup_insert_mode_mapping(&framework_mapping, mapid_global, mapid_file, mapid_code);
    cjh_setup_normal_mode_mapping(&framework_mapping, mapid_normal_mode);
}

#endif //FCODER_CHOGAN_BINDINGS

// BOTTOM

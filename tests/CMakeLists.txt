add_executable(calm_tests
    main.c
    test_strutil.c
    test_strbuf.c
    test_calmconf.c
    test_theme.c
    test_color_scheme.c
    test_emulator.c
    test_alias_table.c
    test_dir_stack.c
    test_history.c
    test_edit_buffer.c
    test_execute.c
)
target_link_libraries(calm_tests PRIVATE calm_core)
target_include_directories(calm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME calm_tests COMMAND calm_tests)

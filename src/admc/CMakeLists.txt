find_package(Qt5 REQUIRED
    COMPONENTS
        Core
        Widgets
        LinguistTools
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(ADMC_APPLICATION_NAME "ADMC")
set(ADMC_APPLICATION_DISPLAY_NAME "ADMC")
set(ADMC_ORGANIZATION "BaseALT")
set(ADMC_ORGANIZATION_DOMAIN "basealt.ru")
configure_file("config.h.in" "config.h")

include_directories(
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src/adldap
)

link_libraries(
    Qt5::Core
    Qt5::Widgets
    adldap
    resolv
)

set(ADMC_SOURCES
    status.cpp
    search_thread.cpp
    globals.cpp
    utils.cpp
    settings.cpp

    main_window.cpp
    main_window_connection_error.cpp
    find_widget.cpp
    tab_widget.cpp
    policy_results_widget.cpp
    edit_query_item_widget.cpp

    policy_ou_results_widget/policy_ou_results_widget.cpp
    policy_ou_results_widget/inherited_policies_widget.cpp
    policy_ou_results_widget/linked_policies_widget.cpp
    policy_ou_results_widget/drag_drop_links_model.cpp

    console_filter_dialog.cpp
    password_dialog.cpp
    rename_object_helper.cpp
    rename_other_dialog.cpp
    rename_policy_dialog.cpp
    rename_user_dialog.cpp
    rename_group_dialog.cpp
    create_object_helper.cpp
    create_user_dialog.cpp
    create_group_dialog.cpp
    create_computer_dialog.cpp
    create_ou_dialog.cpp
    create_shared_folder_dialog.cpp
    create_contact_dialog.cpp
    create_policy_dialog.cpp
    create_query_item_dialog.cpp
    create_query_folder_dialog.cpp
    find_object_dialog.cpp
    select_object_dialog.cpp
    select_object_match_dialog.cpp
    select_object_advanced_dialog.cpp
    select_container_dialog.cpp
    select_policy_dialog.cpp
    about_dialog.cpp
    edit_query_item_dialog.cpp
    edit_query_folder_dialog.cpp
    properties_dialog.cpp
    properties_multi_dialog.cpp
    properties_warning_dialog.cpp
    security_sort_warning_dialog.cpp
    connection_options_dialog.cpp
    changelog_dialog.cpp
    error_log_dialog.cpp
    find_policy_dialog.cpp

    fsmo/fsmo_dialog.cpp
    fsmo/fsmo_tab.cpp
    fsmo/fsmo_utils.cpp

    filter_widget/filter_widget.cpp
    filter_widget/filter_dialog.cpp
    filter_widget/filter_widget_simple_tab.cpp
    filter_widget/filter_widget_normal_tab.cpp
    filter_widget/filter_widget_advanced_tab.cpp
    filter_widget/select_classes_widget.cpp
    filter_widget/class_filter_widget.cpp
    filter_widget/class_filter_dialog.cpp
    filter_widget/select_base_widget.cpp
    
    tabs/general_user_tab.cpp
    tabs/general_group_tab.cpp
    tabs/general_ou_tab.cpp
    tabs/general_computer_tab.cpp
    tabs/general_policy_tab.cpp
    tabs/general_shared_folder_tab.cpp
    tabs/general_other_tab.cpp
    tabs/object_tab.cpp
    tabs/attributes_tab.cpp
    tabs/attributes_tab_proxy.cpp
    tabs/attributes_tab_filter_menu.cpp
    tabs/account_tab.cpp
    tabs/membership_tab.cpp
    tabs/address_tab.cpp
    tabs/organization_tab.cpp
    tabs/telephones_tab.cpp
    tabs/profile_tab.cpp
    tabs/managed_by_tab.cpp
    tabs/security_tab.cpp
    tabs/os_tab.cpp
    tabs/delegation_tab.cpp
    tabs/select_well_known_trustee_dialog.cpp
    tabs/laps_tab.cpp
    tabs/error_tab.cpp
    tabs/group_policy_tab.cpp

    multi_tabs/general_user_multi_tab.cpp
    multi_tabs/general_other_multi_tab.cpp
    multi_tabs/account_multi_tab.cpp
    multi_tabs/address_multi_tab.cpp
    multi_tabs/profile_multi_tab.cpp
    multi_tabs/organization_multi_tab.cpp

    attribute_edits/attribute_edit.cpp
    attribute_edits/string_edit.cpp
    attribute_edits/string_other_edit.cpp
    attribute_edits/string_list_edit.cpp
    attribute_edits/string_large_edit.cpp
    attribute_edits/sam_name_edit.cpp
    attribute_edits/computer_sam_name_edit.cpp
    attribute_edits/country_edit.cpp
    attribute_edits/expiry_edit.cpp
    attribute_edits/expiry_widget.cpp
    attribute_edits/unlock_edit.cpp
    attribute_edits/group_scope_edit.cpp
    attribute_edits/group_type_edit.cpp
    attribute_edits/account_option_edit.cpp
    attribute_edits/password_edit.cpp
    attribute_edits/datetime_edit.cpp
    attribute_edits/gpoptions_edit.cpp
    attribute_edits/manager_edit.cpp
    attribute_edits/upn_edit.cpp
    attribute_edits/upn_suffix_combo.cpp
    attribute_edits/country_combo.cpp
    attribute_edits/manager_widget.cpp
    attribute_edits/delegation_edit.cpp
    attribute_edits/logon_hours_edit.cpp
    attribute_edits/logon_hours_dialog.cpp
    attribute_edits/logon_computers_edit.cpp
    attribute_edits/logon_computers_dialog.cpp
    attribute_edits/protect_deletion_edit.cpp
    attribute_edits/laps_expiry_edit.cpp
    attribute_edits/general_name_edit.cpp
    attribute_edits/dn_edit.cpp

    attribute_edits/upn_multi_edit.cpp
    attribute_edits/account_option_multi_edit.cpp

    attribute_dialogs/attribute_dialog.cpp
    attribute_dialogs/list_attribute_dialog.cpp
    attribute_dialogs/string_attribute_dialog.cpp
    attribute_dialogs/number_attribute_dialog.cpp
    attribute_dialogs/hex_number_attribute_dialog.cpp
    attribute_dialogs/octet_attribute_dialog.cpp
    attribute_dialogs/bool_attribute_dialog.cpp
    attribute_dialogs/datetime_attribute_dialog.cpp
    attribute_dialogs/time_span_attribute_dialog.cpp

    console_widget/console_widget.cpp
    console_widget/scope_proxy_model.cpp
    console_widget/customize_columns_dialog.cpp
    console_widget/results_view.cpp
    console_widget/console_drag_model.cpp
    console_widget/console_impl.cpp

    icon_manager/icon_manager.cpp

    console_impls/object_impl.cpp
    console_impls/policy_impl.cpp
    console_impls/query_item_impl.cpp
    console_impls/query_folder_impl.cpp
    console_impls/policy_root_impl.cpp
    console_impls/find_object_impl.cpp
    console_impls/find_policy_impl.cpp
    console_impls/all_policies_folder_impl.cpp
    console_impls/policy_ou_impl.cpp
    console_impls/found_policy_impl.cpp
    console_impls/domain_info_impl.cpp

    domain_info_results_widget/domain_info_results_widget.cpp

    admc.qrc
)

add_executable(admc
    main.cpp
    ${ADMC_SOURCES}
)
target_clangformat_setup(admc)

# NOTE: build admc as library to streamline building tests.
# Tests just link to this lib and use all of admc's code.
add_library(admctest SHARED
    ${ADMC_SOURCES}
)

set(TS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/admc_ru.ts
    ${CMAKE_CURRENT_SOURCE_DIR}/admc_en.ts
)

# NOTE: custom target to update translation sources (.ts).
# By default, translation sources aren't updated so you have
# to manually make this target to commit translation
# changes.
add_custom_target(admc_ts
    DEPENDS ${TS_FILES}
    COMMAND ${Qt5_LUPDATE_EXECUTABLE} -recursive ${CMAKE_CURRENT_SOURCE_DIR} -ts ${TS_FILES} -noobsolete
)

# Generate translation binaries (.qm) from sources (.ts)
set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION ${CMAKE_CURRENT_SOURCE_DIR})
qt5_add_translation(QM_FILES ${TS_FILES})

install(TARGETS admc DESTINATION ${CMAKE_INSTALL_BINDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
if(BUILD_TESTING)
    install(TARGETS admctest DESTINATION ${CMAKE_INSTALL_LIBDIR}
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif(BUILD_TESTING)

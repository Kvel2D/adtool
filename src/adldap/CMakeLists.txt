find_package(Qt5 REQUIRED
    COMPONENTS
        Core
        LinguistTools
)

find_package(Ldap REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(NdrStandard REQUIRED IMPORTED_TARGET ndr_standard)
pkg_check_modules(Smbclient REQUIRED IMPORTED_TARGET smbclient)
pkg_check_modules(Krb5 REQUIRED IMPORTED_TARGET krb5)
pkg_check_modules(Uuid REQUIRED IMPORTED_TARGET uuid)
pkg_check_modules(Sasl REQUIRED IMPORTED_TARGET libsasl2)

pkg_check_modules(Ndr REQUIRED IMPORTED_TARGET ndr)
if(Ndr_VERSION VERSION_GREATER "1.0.1")
    set(NDR_VERSION_GREATER_THAN_1_0_1 ON)
endif()
configure_file("adldap_config.h.in" "adldap_config.h")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(ADLDAP_SOURCES
    ad_interface.cpp
    ad_config.cpp
    ad_utils.cpp
    ad_object.cpp
    ad_display.cpp
    ad_filter.cpp
    ad_security.cpp
    gplink.cpp
)
prefix_clangformat_setup(adldap ${ADLDAP_SOURCES})

# Bandaid to provide compatibility for Samba 4.20+ versions.
set(VERSION_H "/usr/include/samba-4.0/samba/version.h")
if (EXISTS ${VERSION_H})
    file(READ ${VERSION_H} VERSION_HEADER_CONTENT)

    string(REGEX MATCH "#define SAMBA_VERSION_MINOR [0-9]+" M_VERSION_LINE "${VERSION_HEADER_CONTENT}")
    string(REGEX MATCH "[0-9]+" VERSION_SMB_MINOR "${M_VERSION_LINE}")
    set(SMB_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/samba)
    if (VERSION_SMB_MINOR)
        file(REMOVE ${SMB_SRC_PATH}/ndr_misc.c ${SMB_SRC_PATH}/ndr_misc.h)
    endif(VERSION_SMB_MINOR)

    if (VERSION_SMB_MINOR GREATER_EQUAL 20)
        file(COPY ${SMB_SRC_PATH}/src_4_20/ndr_misc.h ${SMB_SRC_PATH}/src_4_20/ndr_misc.c
             DESTINATION ${SMB_SRC_PATH})
    else()
        file(COPY ${SMB_SRC_PATH}/src_older/ndr_misc.h ${SMB_SRC_PATH}/src_older/ndr_misc.c
             DESTINATION ${SMB_SRC_PATH})
    endif(VERSION_SMB_MINOR GREATER_EQUAL 20)
else()
    message(WARNING "Failed to find Samba version. If its version is 20 or greater, update ndr_misc.* files.")
endif(EXISTS ${VERSION_H})


set(ADLDAP_SAMBA_SOURCES
    samba/gp_manage.c
    samba/ndr_security.c
    samba/dom_sid.c
    samba/replace.c
    samba/ndr_misc.c
    samba/libsmb_xattr.c
    samba/security_descriptor.c
)


add_library(adldap SHARED
    ${ADLDAP_SOURCES}
    ${ADLDAP_SAMBA_SOURCES}
    adldap.qrc
)

target_include_directories(adldap PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(adldap
    Qt5::Core
    PkgConfig::Uuid
    PkgConfig::Smbclient
    PkgConfig::Krb5
    Ldap::Ldap
    PkgConfig::NdrStandard
    PkgConfig::Sasl
    resolv
)

set(TS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/adldap_ru.ts
    ${CMAKE_CURRENT_SOURCE_DIR}/adldap_en.ts
)

# NOTE: custom target to update translation sources (.ts).
# By default, translation sources aren't updated so you have
# to manually make this target to commit translation
# changes.
add_custom_target(adldap_ts
    DEPENDS ${TS_FILES}
    COMMAND ${Qt5_LUPDATE_EXECUTABLE} -recursive ${CMAKE_CURRENT_SOURCE_DIR} -ts ${TS_FILES} -noobsolete
)

if(BUILD_TESTING)
    target_compile_definitions(adldap PRIVATE SMB_HAS_NEW_NDR_PULL_STEAL_SWITCH=1)
endif(BUILD_TESTING)

# Generate translation binaries (.qm) from sources (.ts)
set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION ${CMAKE_CURRENT_SOURCE_DIR})
qt5_add_translation(QM_FILES ${TS_FILES})

install(TARGETS adldap DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

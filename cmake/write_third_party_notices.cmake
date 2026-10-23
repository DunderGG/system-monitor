# Writes OUTPUT, the licenses and notices of the third-party code that ships
# with the executable in DIRECTORY (DC-138): the copyright file that vcpkg
# installed for each port whose DLL is in DIRECTORY or below it, and for each
# port in COMPILED_IN_PORTS, whose code is compiled into the executable. The
# Microsoft C++ runtime is named without a notice. A DLL that no port installed
# fails the script, so that no DLL ships without its license.
#
#   cmake -D DIRECTORY=<dir> -D VCPKG_INSTALLED_DIR=<dir> -D VCPKG_TARGET_TRIPLET=<triplet>
#         -D COMPILED_IN_PORTS=<port>[;<port>...] -D OUTPUT=<file> -P write_third_party_notices.cmake

# vcpkg lists the files of each port it installed in <port>_<version>_<triplet>.list.
file(GLOB portLists "${VCPKG_INSTALLED_DIR}/vcpkg/info/*_${VCPKG_TARGET_TRIPLET}.list")
if(NOT portLists)
    message(FATAL_ERROR "vcpkg has listed no packages in ${VCPKG_INSTALLED_DIR}/vcpkg/info.")
endif()
foreach(portList IN LISTS portLists)
    cmake_path(GET portList STEM LAST_ONLY listName)
    string(REGEX MATCH "^([^_]+)_(.+)_${VCPKG_TARGET_TRIPLET}$" matched "${listName}")
    set(port "${CMAKE_MATCH_1}")
    set("versionOf_${port}" "${CMAKE_MATCH_2}")
    file(STRINGS "${portList}" portLibraries REGEX "\\.dll$")
    foreach(portLibrary IN LISTS portLibraries)
        # A port's tools directory holds copies of other ports' DLLs, such as
        # qtbase's copy of double-conversion.dll for its host tools.
        if(portLibrary MATCHES "^[^/]+/tools/")
            continue()
        endif()
        cmake_path(GET portLibrary FILENAME name)
        string(TOLOWER "${name}" name)
        set("portOf_${name}" "${port}")
    endforeach()
endforeach()

set(ports ${COMPILED_IN_PORTS})
set(runtimeLibraries "")
file(GLOB_RECURSE shippedLibraries LIST_DIRECTORIES false RELATIVE "${DIRECTORY}" "${DIRECTORY}/*.dll")
foreach(library IN LISTS shippedLibraries)
    cmake_path(GET library FILENAME name)
    string(TOLOWER "${name}" key)
    if(DEFINED "portOf_${key}")
        list(APPEND ports "${portOf_${key}}")
    elseif(key MATCHES "^(msvcp|vcruntime|concrt)[0-9]")
        list(APPEND runtimeLibraries "${name}")
    else()
        message(FATAL_ERROR "No vcpkg package installed ${DIRECTORY}/${library}, so THIRD_PARTY_NOTICES.txt "
                            "cannot give its license.")
    endif()
endforeach()
list(REMOVE_DUPLICATES ports)
list(SORT ports CASE INSENSITIVE)
list(SORT runtimeLibraries CASE INSENSITIVE)

set(notices [[
Third-party notices for Verdandi

Verdandi is licensed under the GNU General Public License, version 3, which
LICENSE.txt holds. It uses the open-source packages below, which vcpkg built.
Each section names a package and its version, followed by its licenses and
notices as the package gives them.
]])
if(runtimeLibraries)
    list(JOIN runtimeLibraries "\n    " runtimeLibraryNames)
    string(APPEND notices "\nThese Microsoft C++ runtime libraries are distributed under the license terms\n"
                          "of Microsoft Visual Studio for its redistributable code:\n\n    ${runtimeLibraryNames}\n")
endif()

string(REPEAT "=" 79 rule)
foreach(port IN LISTS ports)
    set(copyrightFile "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/${port}/copyright")
    if(NOT EXISTS "${copyrightFile}")
        message(FATAL_ERROR "vcpkg installed no copyright file for ${port}: ${copyrightFile}")
    endif()
    file(READ "${copyrightFile}" copyright)
    string(APPEND notices "\n${rule}\n${port} ${versionOf_${port}}\n${rule}\n\n${copyright}\n")
endforeach()

file(WRITE "${OUTPUT}" "${notices}")

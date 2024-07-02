# Copyright 2024 Uwe Salomon <post@uwesalomon.de>
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program. If not, see <http://www.gnu.org/licenses/>.
#
find_package(Doxygen)


function(doxygen)
  set(oneValueArgs DOXYFILE)
  set(multiValueArgs SOURCES)
  cmake_parse_arguments(DOXYGEN "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(DOXYGEN_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "Invalid doxygen() arguments ${DOXYGEN_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT DOXYGEN_DOXYFILE)
    message(FATAL_ERROR "Missing doxygen() DOXYFILE")
  endif()

  file(REAL_PATH ${DOXYGEN_DOXYFILE} DOXYGEN_DOXYFILE_PATH)
  file(RELATIVE_PATH DOXYGEN_DOXYFILE_RELPATH ${CMAKE_SOURCE_DIR} ${DOXYGEN_DOXYFILE_PATH})
  set(DOXYGEN_DOXYFILE_2 ${CMAKE_CURRENT_BINARY_DIR}/doxyfile)

  string(CONCAT DOXYGEN_DOXYFILE_2_CONTENT
    "OUTPUT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}\n"
    "GENERATE_LATEX=NO\n"
    "QUIET=YES\n"
    "@INCLUDE=${DOXYGEN_DOXYFILE_PATH}\n")
  file(GENERATE OUTPUT ${DOXYGEN_DOXYFILE_2} CONTENT ${DOXYGEN_DOXYFILE_2_CONTENT})

  if(DOXYGEN_FOUND)
    set(DOXYGEN_HTML_DIR ${CMAKE_CURRENT_BINARY_DIR}/html)
    set(DOXYGEN_HTML_MARK ${CMAKE_CURRENT_BINARY_DIR}/.doxygen-mark)
    set(DOXYGEN_JQUERY_MARK)

    add_custom_command(OUTPUT ${DOXYGEN_HTML_MARK}
      COMMENT "Create doxygen documentation for ${DOXYGEN_DOXYFILE_RELPATH}"
      DEPENDS ${DOXYGEN_DOXYFILE_PATH} ${DOXYGEN_SOURCES}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      BYPRODUCTS ${DOXYGEN_HTML_DIR}
      COMMAND PROJECT_VERSION=${CMAKE_PROJECT_VERSION} ${DOXYGEN_EXECUTABLE} ${DOXYGEN_DOXYFILE_2}
      COMMAND ${CMAKE_COMMAND} -E touch ${DOXYGEN_HTML_MARK}
      VERBATIM)

    if(DOXYGEN_JQUERY_PATH)
      find_program(SED sed)
      if(NOT SED)
        message(FATAL_ERROR "Could not find sed")
      endif()

      set(DOXYGEN_JQUERY_MARK ${CMAKE_CURRENT_BINARY_DIR}/.jquery-mark)
      add_custom_command(OUTPUT ${DOXYGEN_JQUERY_MARK}
        COMMENT "Patch jquery path in doxygen documentation"
        DEPENDS ${DOXYGEN_HTML_MARK}
        COMMAND ${SED} -i 's@src="jquery.js"@src="${DOXYGEN_JQUERY_PATH}"@g'
                "${DOXYGEN_HTML_DIR}/*.html"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${DOXYGEN_HTML_DIR}/jquery.js"
        COMMAND ${CMAKE_COMMAND} -E touch ${DOXYGEN_JQUERY_MARK})
    endif()

    add_custom_target(docs ALL
      SOURCES ${DOXYGEN_DOXYFILE} ${DOXYGEN_SOURCES}
      DEPENDS ${DOXYGEN_HTML_MARK} ${DOXYGEN_JQUERY_MARK})
  else()
    message(WARNING "Could not find doxygen, will not generate documentation")
  endif()
endfunction()

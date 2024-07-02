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
find_program(HELP2MAN help2man)


function(help2man)
  set(oneValueArgs TARGET SECTION NAME)
  cmake_parse_arguments(HELP2MAN "" "${oneValueArgs}" "" ${ARGN})

  if(HELP2MAN_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "Invalid help2man() arguments ${HELP2MAN_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT HELP2MAN_TARGET)
    message(FATAL_ERROR "Missing help2man() TARGET")
  endif()

  if(NOT HELP2MAN_SECTION)
    set(HELP2MAN_SECTION 0)
  endif()

  set(HELP2MAN_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${HELP2MAN_TARGET}.${HELP2MAN_SECTION})
  file(RELATIVE_PATH HELP2MAN_OUTPUT_RELPATH ${CMAKE_BINARY_DIR} ${HELP2MAN_OUTPUT})

  set(HELP2MAN_ARGS)
  if(HELP2MAN_NAME)
    set(HELP2MAN_ARGS ${HELP2MAN_ARGS} --name="${HELP2MAN_NAME}")
  endif()

  if(HELP2MAN)
    add_custom_command(OUTPUT ${HELP2MAN_OUTPUT}
      COMMENT "Create manpage ${HELP2MAN_OUTPUT_RELPATH}"
      DEPENDS ${HELP2MAN_TARGET}
      COMMAND ${HELP2MAN} ${HELP2MAN_ARGS}
              --section=${HELP2MAN_SECTION}
              --output="${HELP2MAN_OUTPUT}"
              --no-info
              $<TARGET_FILE:${HELP2MAN_TARGET}>)

    add_custom_target(${HELP2MAN_TARGET}-manpage DEPENDS ${HELP2MAN_OUTPUT})
    add_custom_target(manpages ALL DEPENDS ${HELP2MAN_TARGET}-manpage)

    install(FILES ${HELP2MAN_OUTPUT}
      DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man${HELP2MAN_SECTION})
  else()
    message(WARNING "Could not find help2man, will not generate manpages")
  endif()
endfunction()

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
option(GENERATE_COVERAGE "Enable coverage data generation")

set(COVERAGE_COMPILER_FLAGS "--coverage -fprofile-abs-path"
  CACHE INTERNAL "Compiler flags to enable coverage data generation")



function(target_enable_coverage target)
  if(GENERATE_COVERAGE)
    separate_arguments(COVERAGE_COMPILER_FLAG_LIST NATIVE_COMMAND "${COVERAGE_COMPILER_FLAGS}")
    target_compile_options(${target} PRIVATE ${COVERAGE_COMPILER_FLAG_LIST})
    target_link_options(${target} PRIVATE ${COVERAGE_COMPILER_FLAG_LIST})
  endif()
endfunction()

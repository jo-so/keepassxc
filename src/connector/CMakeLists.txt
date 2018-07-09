#  Copyright (C) 2017 Sami Vänttinen <sami.vanttinen@protonmail.com>
#  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
#  Copyright © 2018 Jörg Sommer <joerg@alea.gnuu.de>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

if(WITH_XC_BROWSER)
    find_package(sodium 1.0.12 REQUIRED)

    set(connector_SOURCES
      keepassxc-connector.cpp
      AppBase.cpp
      GenericApp.cpp
      ProxyApp.cpp
      )

    add_executable(keepassxc-connector ${connector_SOURCES})
    target_link_libraries(keepassxc-connector Qt5::Core Qt5::Network sodium)

    install(TARGETS keepassxc-connector
            BUNDLE DESTINATION . COMPONENT Runtime
            RUNTIME DESTINATION ${CLI_INSTALL_DIR} COMPONENT Runtime)

    if(MINGW)
      target_link_libraries(keepassxc-connector Wtsapi32.lib Ws2_32.lib)
    endif()
endif()

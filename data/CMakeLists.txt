configure_file(whosthere.service.in org.freedesktop.Telepathy.ConnectionManager.whosthere.service)

install (FILES whosthere.manager DESTINATION share/telepathy/managers)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.Telepathy.ConnectionManager.whosthere.service DESTINATION share/dbus-1/services)
install (FILES whosthere.png DESTINATION share/icons/hicolor/48x48/apps)

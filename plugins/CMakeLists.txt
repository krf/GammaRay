include_directories(
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/3rdparty
)

add_subdirectory(actioninspector)
add_subdirectory(objectvisualizer)

if(QT_QTSCRIPTTOOLS_FOUND OR Qt5ScriptTools_FOUND)
  add_subdirectory(scriptenginedebugger)
endif()

add_subdirectory(sceneinspector)
add_subdirectory(statemachineviewer)
add_subdirectory(styleinspector)
add_subdirectory(timertop)
if(NOT Qt5Core_FOUND)
  add_subdirectory(kjobtracker)
endif()

if(QT_QTWEBKIT_FOUND OR Qt5WebKitWidgets_FOUND)
  add_subdirectory(webinspector)
endif()

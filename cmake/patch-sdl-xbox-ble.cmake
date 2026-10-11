if(NOT DEFINED SDL_SOURCE_DIR)
    message(FATAL_ERROR "SDL_SOURCE_DIR is required")
endif()

set(SDL_XBOX_HID_FILE "${SDL_SOURCE_DIR}/src/joystick/hidapi/SDL_hidapi_xboxone.c")
file(READ "${SDL_XBOX_HID_FILE}" SDL_XBOX_HID_CONTENT)
set(SDL_XBOX_HID_ANCHOR [=[    if (device && SDL_strncmp(device->path, "\\\\?\\HID#", 8) == 0) {]=])
set(TOU_XBOX_HID_REPLACEMENT [=[    /* TOU: the Series BLE endpoint is real Bluetooth HID, not a wired XGIP proxy. */
    if (device && SDL_strncmp(device->path, "\\\\?\\HID#", 8) == 0 &&
        !(vendor_id == 0x045e && product_id == 0x0b13 && device->is_bluetooth)) {]=])
string(FIND "${SDL_XBOX_HID_CONTENT}" "${TOU_XBOX_HID_REPLACEMENT}" TOU_XBOX_HID_POSITION)
if(TOU_XBOX_HID_POSITION EQUAL -1)
    string(FIND "${SDL_XBOX_HID_CONTENT}" "${SDL_XBOX_HID_ANCHOR}" SDL_XBOX_HID_POSITION)
    if(SDL_XBOX_HID_POSITION EQUAL -1)
        message(FATAL_ERROR "Pinned SDL Xbox HID endpoint filter changed")
    endif()
    string(REPLACE "${SDL_XBOX_HID_ANCHOR}" "${TOU_XBOX_HID_REPLACEMENT}"
        SDL_XBOX_HID_CONTENT "${SDL_XBOX_HID_CONTENT}")
    file(WRITE "${SDL_XBOX_HID_FILE}" "${SDL_XBOX_HID_CONTENT}")
endif()

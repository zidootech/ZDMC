set(SOURCES ConsoleDeviceKitPowerSyscall.cpp
            ConsoleUPowerSyscall.cpp
            UPowerSyscall.cpp
            LogindUPowerSyscall.cpp)

set(HEADERS ConsoleDeviceKitPowerSyscall.h
            ConsoleUPowerSyscall.h
            FallbackPowerSyscall.h
            LogindUPowerSyscall.h
            UPowerSyscall.h)

if(MMAL_FOUND)
  list(APPEND SOURCES RaspberryPIPowerSyscall.cpp)
  list(APPEND HEADERS RaspberryPIPowerSyscall.h)
endif()

core_add_library(powermanagement_linux)

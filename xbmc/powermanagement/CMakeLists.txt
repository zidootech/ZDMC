set(SOURCES DPMSSupport.cpp
            IPowerSyscall.cpp
            PowerManager.cpp)

set(HEADERS DPMSSupport.h
            IPowerSyscall.h
            PowerManager.h)

if(MMAL_FOUND)
  list(APPEND SOURCES PowerSyscallVirtualSleep.cpp)
  list(APPEND HEADERS PowerSyscallVirtualSleep.h)
endif()

core_add_library(powermanagement)

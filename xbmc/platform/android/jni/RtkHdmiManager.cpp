#include "JNIBase.h"

#include "RtkHdmiManager.h"

#include "jutils/jutils-details.hpp"

using namespace jni;

//com.realtek.hardware
CJNIRtkHdmiManager::CJNIRtkHdmiManager() : CJNIBase("com/realtek/hardware/RtkHDMIManager")
{
  m_object = new_object(GetClassName(), "<init>", "()V");
  m_object.setGlobal();
}

bool CJNIRtkHdmiManager::checkIfHDMIPlugged()
{
  return call_method<jboolean>(m_object,
    "checkIfHDMIPlugged", "()Z");	
}

bool CJNIRtkHdmiManager::checkIfHDMIReady()
{
	return call_method<jboolean>(m_object,
		"checkIfHDMIReady", "()Z");	
}

std::vector<int> CJNIRtkHdmiManager::getVideoFormat() const
{
  return jcast<std::vector<int>>(
    call_method<jhintArray>(m_object, "getVideoFormat", "()[I"));
}

int CJNIRtkHdmiManager::setTVSystem(int tvSystem)
{
	return call_method<jint>(m_object,
		"setTVSystem", "(I)I", tvSystem);		
}

int CJNIRtkHdmiManager::getTVSystem()
{
	return call_method<jint>(m_object,
		"getTVSystem", "()I");		
}
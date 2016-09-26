#pragma once


#include "JNIBase.h"


class CJNIRtkHdmiManager : public CJNIBase
{
public:
  CJNIRtkHdmiManager();
  ~CJNIRtkHdmiManager() {};
	
	bool checkIfHDMIPlugged();
	bool checkIfHDMIReady();
  std::vector<int> getVideoFormat() const;
	int setTVSystem(int tvSystem);
	int getTVSystem();
};	
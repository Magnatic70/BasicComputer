// v001:	* First version
// v002:	* Changed from magnatic-esp8266 to magnatic-esp

#include "magnatic-esp.h"
#include "bcbasic.h"

void setup(){
	pinMode(ActivePin,OUTPUT);
	digitalWrite(ActivePin,LOW);
	magnaticDeviceDescription="BasicComputer";
	magnaticDeviceType="BASCOMP";
	magnaticDeviceTypeIndexFileSize=706;
	getDeviceReleaseFromSourceFile(__FILE__);

	espSetup();
	basicSetup();
}

void loop(){
	basicLoop(); // The espLoop is performed within the basicLoop
}

#include <Bela.h>
#include <vector>
#include <string>

#include "../include/IDisplayContext.h"
#include "../include/DisplayContextFake.h"

void DisplayContextFake::setLines(int displayNumber, int lineNumber, std::string line0, std::string line1 , std::string line2, std::string line3)  {
	lines.at(displayNumber).at(lineNumber) = line0;
	if(line1 != "") lines.at(displayNumber).at(lineNumber + 1) = line1;
	if(line2 != "") lines.at(displayNumber).at(lineNumber + 2) = line2;
	if(line3 != "") lines.at(displayNumber).at(lineNumber + 3) = line3;
	this->renderDisplay();
}

void DisplayContextFake::setLines(std::vector<std::vector<std::string>> lines){
	this->lines = lines;
}


void DisplayContextFake::setProgress(int displayNumber, float percentage)  {
	lines.at(displayNumber).at(3) = "progress: " + std::to_string(percentage);
	this->renderDisplay();
}

std::atomic<bool>& DisplayContextFake::getUpdateDisplayFlag()  {
		rt_printf("ERROR: trying to getUpdateDisplayFlag() pointer, but here we have a fake display context\n");
		throw std::runtime_error("ERROR: trying to getUpdateDisplayFlag() pointer, but here we have a fake display context\n");
}

AuxiliaryTask& DisplayContextFake::getDisplayTask()  {
	rt_printf("ERROR: trying to getDisplayTask() pointer, but here we have a fake display context\n");
	throw std::runtime_error("ERROR: trying to getDisplayTask() pointer, but here we have a fake display context\n");
}

void DisplayContextFake::renderDisplay()  {
	rt_printf("############################################\n");
	for(int displayNumber = 0; displayNumber < 2; displayNumber++){
		rt_printf("--------------------\n");
		for(int lineNumber = 0; lineNumber < 4; lineNumber++){
			rt_printf((lines.at(displayNumber).at(lineNumber) + "\n").c_str());
		}
	}
}
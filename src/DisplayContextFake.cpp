#include <Bela.h>
#include <vector>
#include <string>
#include <cassert>

#include "../include/IDisplayContext.h"
#include "../include/DisplayContextFake.h"

void DisplayContextFake::setLines(int displayNumber, int lineNumber, std::string line0, std::string line1 , std::string line2, std::string line3)  {
	assert(displayNumber >= 0 && lines.size() > static_cast<size_t>(displayNumber));
	auto& displayLines = lines.at(displayNumber);
	assert(lineNumber >= 0 && displayLines.size() > static_cast<size_t>(lineNumber));
	displayLines.at(lineNumber) = line0;
	if(line1 != ""){
		assert(lineNumber + 1 >= 0 && displayLines.size() > static_cast<size_t>(lineNumber + 1));
		displayLines.at(lineNumber + 1) = line1;
	}
	if(line2 != ""){
		assert(lineNumber + 2 >= 0 && displayLines.size() > static_cast<size_t>(lineNumber + 2));
		displayLines.at(lineNumber + 2) = line2;
	}
	if(line3 != ""){
		assert(lineNumber + 3 >= 0 && displayLines.size() > static_cast<size_t>(lineNumber + 3));
		displayLines.at(lineNumber + 3) = line3;
	}
	this->renderDisplay();
}

void DisplayContextFake::setLines(std::vector<std::vector<std::string>> lines){
	this->lines = lines;
}


void DisplayContextFake::setProgress(int displayNumber, float percentage)  {
	assert(displayNumber >= 0 && lines.size() > static_cast<size_t>(displayNumber));
	auto& displayLines = lines.at(displayNumber);
	assert(displayLines.size() > 3);
	displayLines.at(3) = "progress: " + std::to_string(percentage);
	this->renderDisplay();
}

TaskWrapper& DisplayContextFake::getDisplayTask()  {
	rt_printf("ERROR: trying to getDisplayTask() pointer, but here we have a fake display context\n");
	throw std::runtime_error("ERROR: trying to getDisplayTask() pointer, but here we have a fake display context\n");
}

void DisplayContextFake::renderDisplay()  {
	rt_printf("############################################\n");
	for(int displayNumber = 0; displayNumber < 2; displayNumber++){
		rt_printf("--------------------\n");
		for(int lineNumber = 0; lineNumber < 4; lineNumber++){
			assert(lines.size() > static_cast<size_t>(displayNumber));
			auto& displayLines = lines.at(displayNumber);
			assert(displayLines.size() > static_cast<size_t>(lineNumber));
			rt_printf((displayLines.at(lineNumber) + "\n").c_str());
		}
	}
}

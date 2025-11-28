#pragma once

#include <Bela.h>
#include<vector>
#include<string>
#include<stdexcept>
#include<atomic>
#include <cassert>

#include "IDisplayContext.h"

class DisplayContextFake: public IDisplayContext {
public:
    DisplayContextFake() { rt_printf("constructing fake display context\n");}
    
    void initDisplayContext() override {rt_printf("initializing fake displaycontext\n");}
    
    void processBlockwise() override {
    	//nothing to be done, 
    }
	
	void setLines(int displayNumber, int lineNumber, std::string line0, std::string line1 = "", std::string line2 = "", std::string line3 = "") override;
	
	void setLines(std::vector<std::vector<std::string>>lines) override;

	std::string getLine(int displayNumber, int lineNumber) override {
		assert(displayNumber >= 0 && lines.size() > static_cast<size_t>(displayNumber));
		auto& displayLines = lines.at(displayNumber);
		assert(lineNumber >= 0 && displayLines.size() > static_cast<size_t>(lineNumber));
		return displayLines.at(lineNumber);
	}
	
	void setProgress(int displayNumber, float percentage) override;
	
	AuxiliaryTask& getDisplayTask() override;
	
	void renderDisplay() override;
	
private:
	std::vector<std::vector<std::string>> lines = std::vector<std::vector<std::string>>{std::vector<std::string>(4,""), std::vector<std::string>(4,"")};
	
};

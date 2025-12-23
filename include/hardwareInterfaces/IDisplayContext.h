#pragma once
#include<vector>
#include<string>
#include<stdexcept>
#include<atomic>

class IDisplayContext {
public:
    
    virtual ~IDisplayContext() = default;
    
    virtual void initDisplayContext () = 0;
    
    virtual void processBlockwise () = 0;
	
	virtual void setLines(int, int, std::string, std::string line1 = "", std::string line2 = "", std::string line3 = "") = 0;
	
	virtual void setLines(std::vector<std::vector<std::string>>) = 0;

	virtual std::string getLine(int, int) = 0;
	
	virtual void setProgress(int, float) = 0;
		
	virtual void renderDisplay() = 0;
	
};



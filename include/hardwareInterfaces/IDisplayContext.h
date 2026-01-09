#pragma once
#include<vector>
#include<string>
#include<stdexcept>
#include<atomic>
//compiel

struct TextFrame {
    int startChar;
    int textLine;
};
class IDisplayContext {
public:
    
    virtual ~IDisplayContext() = default;
    
    virtual void initDisplayContext () = 0;
    
    virtual void processBlockwise () = 0;
		
	virtual void setLines(std::vector<std::vector<std::string>>) = 0;

	virtual void setLines(std::vector<std::vector<std::string>> lines, std::vector<std::vector<TextFrame>> textFrames) = 0;

	virtual std::string getLine(int, int) = 0;
	
	virtual void setProgress(int, float) = 0;
		
	virtual void renderDisplay() = 0;
	
};



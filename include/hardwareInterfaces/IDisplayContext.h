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

struct ScrollBar {
    bool enabled = false;
    int totalItems = 0;
    int visibleItems = 0;
    int selectedIndex = 0;
};
class IDisplayContext {
public:
    
    virtual ~IDisplayContext() = default;
    
    virtual void initDisplayContext () = 0;
    
    virtual void processBlockwise () = 0;
		
	virtual void setLines(std::vector<std::vector<std::string>>) = 0;

	virtual void setLines(std::vector<std::vector<std::string>> lines, std::vector<std::vector<TextFrame>> textFrames) = 0;
	
	virtual void setLines(std::vector<std::vector<std::string>> lines,
						  std::vector<std::vector<TextFrame>> textFrames,
						  std::vector<ScrollBar> scrollBars) = 0;

	virtual std::string getLine(int, int) = 0;
	
	virtual void setProgress(int, float) = 0;
		
	virtual void renderDisplay() = 0;
	
};


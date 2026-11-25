CXX = g++
WINDRES = windres
CXXFLAGS = -std=c++17 -O2 -ffunction-sections -fdata-sections -I"." -I"Include" -I"GLFW" -I"GLFW/Include" -I"ImGUI" -I"ImGUI/backends" -I"Scintilla/Include" -I"Lexilla/Include" -I"$(VULKAN_SDK)/Include"
LDFLAGS = -mwindows -Wl,--gc-sections -s -L"GLFW/Lib" -L"$(VULKAN_SDK)/Lib" -lglfw3dll -lvulkan-1 -lgdi32 -lshell32 -lole32 -luuid -lcomdlg32 -lwinhttp

OBJDIR = ObjFiles
SOURCES = Src/Main.cpp Src/Editor.cpp Src/EditorSidebar.cpp Src/ScintillaHost.cpp Src/EditorFeatures.cpp Src/LspClient.cpp Src/DebuggerClient.cpp ImGUI/imgui.cpp ImGUI/imgui_draw.cpp ImGUI/imgui_tables.cpp ImGUI/imgui_widgets.cpp ImGUI/backends/imgui_impl_glfw.cpp ImGUI/backends/imgui_impl_vulkan.cpp
OBJECTS = $(addprefix $(OBJDIR)/, $(notdir $(SOURCES:.cpp=.o))) $(OBJDIR)/VadexRes.o
HEADERS = Include/Editor.hpp Include/Icon.hpp Include/Theme.hpp Include/ScintillaHost.hpp Include/EditorFeatures.hpp Include/Minimap.hpp Include/StickyScroll.hpp Include/CommandPalette.hpp Include/DiffViewer.hpp Include/LspClient.hpp Include/ConfigManager.hpp Include/WorkspaceSearch.hpp Include/GitManager.hpp Include/SymbolOutline.hpp Include/IntelliSense.hpp Include/DebuggerClient.hpp Include/MarkdownPreview.hpp

TARGET = CodeEditor.exe

vpath %.cpp Src ImGUI ImGUI/backends

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	rm -f Build/$(TARGET)
	mv $(TARGET) Build/
	cp GLFW/lib/glfw3.dll Build/
	cp Scintilla/Bin/Scintilla.dll Build/
	cp Lexilla/Bin/lexilla.dll Build/

$(OBJDIR)/VadexRes.o: Assets/Vadex.rc Assets/Images/Vadex.ico | $(OBJDIR)
	$(WINDRES) Assets/Vadex.rc -O coff -o $@

$(OBJDIR)/Main.o: Src/Main.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/Editor.o: Src/Editor.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/EditorSidebar.o: Src/EditorSidebar.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/ScintillaHost.o: Src/ScintillaHost.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/EditorFeatures.o: Src/EditorFeatures.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/LspClient.o: Src/LspClient.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/DebuggerClient.o: Src/DebuggerClient.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	@if not exist $(OBJDIR) mkdir $(OBJDIR) 2>nul || mkdir -p $(OBJDIR) 2>nul || true

run:
	Build/./$(TARGET)

clean:
	rm -f Build/$(TARGET) $(OBJDIR)/*.o

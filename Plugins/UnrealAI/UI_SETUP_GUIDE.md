# UnrealAI Plugin - UI Setup Guide

## Overview

The UnrealAI plugin now includes a comprehensive, modern UI system. This guide will help you set up the UMG widget interface.

## Current Status

✅ **Plugin compiled successfully**  
✅ **AI Service working**  
✅ **C++ Widget class created**  
⚠️ **UMG Widget Blueprint needs to be created manually**

## UI Features

The new UI includes:

### 🎨 **Modern Design**
- Clean, professional interface
- Responsive layout
- Status indicators with icons
- Progress bars and loading states

### 📝 **Input Section**
- Large text input area for prompts
- Request type dropdown (General, Blueprint, C++, Code Review, Documentation)
- AI provider selection (Local LLM, Claude, OpenAI)
- Advanced options toggle

### ⚙️ **Advanced Options**
- Temperature slider (0.0 - 2.0)
- Max tokens spinbox (100 - 8192)
- Real-time value display

### 🔘 **Action Buttons**
- **Generate Blueprint** - Create Blueprint logic from description
- **Generate C++** - Generate C++ code from description
- **Analyze Code** - Review and analyze existing code
- **General Query** - Ask general questions about Unreal Engine
- **Clear** - Clear input and response
- **Settings** - Access plugin settings

### 📊 **Response Section**
- Scrollable response area
- Copy to clipboard functionality
- Save response to file
- Status indicators
- Processing time display

## Setting Up the UMG Widget

### Step 1: Create the Widget Blueprint

1. **Open Unreal Editor**
2. **Right-click in Content Browser** → **User Interface** → **Widget Blueprint**
3. **Name it** `WBP_UnrealAI`
4. **Set Parent Class** to `UnrealAIWidget` (our C++ class)

### Step 2: Design the Layout

Create this hierarchy in the Widget Blueprint:

```
WBP_UnrealAI (UnrealAIWidget)
├── MainBorder (Border)
│   └── MainVerticalBox (Vertical Box)
│       ├── TitleText (Text Block) - "🤖 UnrealAI Assistant"
│       ├── StatusRow (Horizontal Box)
│       │   ├── StatusIcon (Image)
│       │   └── StatusText (Text Block) - "Ready"
│       ├── InputSection (Vertical Box)
│       │   ├── PromptLabel (Text Block) - "Enter your prompt:"
│       │   ├── PromptInput (Editable Text Box) - Large input area
│       │   ├── OptionsRow (Horizontal Box)
│       │   │   ├── RequestTypeCombo (Combo Box String)
│       │   │   └── ProviderCombo (Combo Box String)
│       │   └── EnableAdvancedOptions (Check Box) - "Advanced Options"
│       ├── AdvancedOptionsSection (Vertical Box)
│       │   ├── TemperatureRow (Horizontal Box)
│       │   │   ├── TemperatureLabel (Text Block) - "Temperature:"
│       │   │   ├── TemperatureSlider (Slider)
│       │   │   └── TemperatureValue (Text Block) - "0.70"
│       │   └── MaxTokensRow (Horizontal Box)
│       │       ├── MaxTokensLabel (Text Block) - "Max Tokens: 2048"
│       │       └── MaxTokensSpinBox (Spin Box)
│       ├── ActionButtonsRow (Horizontal Box)
│       │   ├── GenerateBlueprintButton (Button) - "🔧 Generate Blueprint"
│       │   ├── GenerateCPPButton (Button) - "💻 Generate C++"
│       │   ├── AnalyzeCodeButton (Button) - "🔍 Analyze Code"
│       │   ├── GeneralQueryButton (Button) - "❓ General Query"
│       │   ├── ClearButton (Button) - "🗑️ Clear"
│       │   └── SettingsButton (Button) - "⚙️ Settings"
│       ├── ProgressBar (Progress Bar) - Hidden by default
│       └── ResponseSection (Vertical Box)
│           ├── ResponseHeaderRow (Horizontal Box)
│           │   ├── ResponseLabel (Text Block) - "AI Response:"
│           │   ├── CopyResponseButton (Button) - "📋 Copy"
│           │   └── SaveResponseButton (Button) - "💾 Save"
│           └── ResponseScrollBox (Scroll Box)
│               └── ResponseText (Text Block) - Response content
```

### Step 3: Configure Widget Bindings

**Important**: Make sure all widget names match exactly with the C++ class:

- `MainBorder`
- `MainVerticalBox`
- `TitleText`
- `StatusText`
- `InputSection`
- `PromptLabel`
- `PromptInput`
- `OptionsRow`
- `RequestTypeCombo`
- `ProviderCombo`
- `EnableAdvancedOptions`
- `AdvancedOptionsSection`
- `TemperatureRow`
- `TemperatureLabel`
- `TemperatureSlider`
- `TemperatureValue`
- `MaxTokensRow`
- `MaxTokensLabel`
- `MaxTokensSpinBox`
- `ActionButtonsRow`
- `GenerateBlueprintButton`
- `GenerateCPPButton`
- `AnalyzeCodeButton`
- `GeneralQueryButton`
- `ClearButton`
- `SettingsButton`
- `ProgressBar`
- `StatusRow`
- `StatusIcon`
- `ResponseSection`
- `ResponseHeaderRow`
- `ResponseLabel`
- `CopyResponseButton`
- `SaveResponseButton`
- `ResponseScrollBox`
- `ResponseText`

### Step 4: Style the Widget

#### Colors and Styling
- **Background**: Dark theme with subtle borders
- **Buttons**: Modern flat design with hover effects
- **Text**: Clear, readable fonts
- **Status Icons**: Green for success, red for errors

#### Layout Sizing
- **Main Window**: 800x600 pixels (resizable)
- **Input Area**: Large enough for detailed prompts
- **Response Area**: Scrollable, takes remaining space
- **Buttons**: Consistent height, responsive width

### Step 5: Test the Widget

1. **Compile the project** (after closing editor or disabling Live Coding)
2. **Click the UnrealAI button** in the toolbar
3. **The window should open** with the full UI
4. **Test all buttons** and functionality

## Troubleshooting

### Widget Not Appearing
- Check that widget names match exactly
- Ensure parent class is set to `UnrealAIWidget`
- Verify the widget blueprint is saved

### Buttons Not Working
- Check event bindings in the widget blueprint
- Ensure C++ functions are properly exposed
- Verify the AI service is initialized

### Compilation Errors
- Close Unreal Editor before building
- Or press `Ctrl+Alt+F11` to disable Live Coding
- Check that all includes are correct

## Next Steps

Once the UI is working:

1. **Test all AI functions** with the new interface
2. **Customize the styling** to match your preferences
3. **Enable HTTP functionality** for real AI responses
4. **Configure API keys** for Claude and OpenAI
5. **Add more features** like response history, templates, etc.

## File Locations

- **C++ Widget Class**: `Plugins/UnrealAI/Source/UnrealAI/UnrealAIWidget.h/cpp`
- **Widget Blueprint**: Create in `Content/UI/WBP_UnrealAI`
- **Plugin Module**: `Plugins/UnrealAI/Source/UnrealAI/UnrealAI.cpp`

## Support

If you encounter issues:
1. Check the console for error messages
2. Verify all widget bindings are correct
3. Ensure the plugin is properly enabled
4. Test with the fallback dialog first

The plugin is designed to gracefully fall back to a simple dialog if the full UI fails to load, so you'll always have a working interface!
